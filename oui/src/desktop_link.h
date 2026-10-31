#pragma once

// A program's end of the conversation with OUI's desktop, shared by its backends of cells and of pixels.

#include "descriptor.h"
#include "desktop_protocol.h"

#include <oui/desktop.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace olagarro::oui
{

namespace protocol = desktop_protocol;

[[noreturn]] inline void desktop_failure(const std::string& what)
{
    throw std::system_error{errno, std::generic_category(), "Cannot show on the desktop: " + what};
}

// A program's conversation with the desktop, whatever its frames are made of: it says hello and waits for its size,
// passes on the input that comes, shares the clipboard, and sends frames through shared memory, one at a time.
class DesktopLink
{
  public:
    explicit DesktopLink(DesktopClientOptions options)
    {
        if(options.socket.empty())
        {
            const auto* const variable{std::getenv(desktop_variable)};
            if(variable == nullptr || *variable == '\0')
            {
                errno = ENOENT;
                desktop_failure(std::string{desktop_variable} + " is not set");
            }
            options.socket = variable;
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const auto path{options.socket.string()};
        if(path.size() >= sizeof address.sun_path)
        {
            errno = ENAMETOOLONG;
            desktop_failure(path);
        }
        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
        m_socket = Descriptor{::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0)};
        if(m_socket.get() < 0 ||
           ::connect(m_socket.get(), reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            desktop_failure(path);
        }
        m_wake = Descriptor{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
        protocol::send(m_socket.get(), protocol::Message::hello, options.title);
        // The program lays itself out in the size it is given, so it waits for it.
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
        while(!m_configured && !m_gone)
        {
            const auto left{
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())};
            if(left.count() <= 0)
            {
                errno = ETIMEDOUT;
                desktop_failure("the desktop does not answer");
            }
            pollfd watched{m_socket.get(), POLLIN, 0};
            ::poll(&watched, 1, static_cast<int>(left.count()));
            read_messages();
        }
        if(m_gone)
        {
            errno = ECONNRESET;
            desktop_failure("the desktop went");
        }
    }

    DesktopLink(const DesktopLink&) = delete;
    DesktopLink& operator=(const DesktopLink&) = delete;

    ~DesktopLink() { unmap(); }

    [[nodiscard]] bool shows_cells() const { return m_cells; }
    [[nodiscard]] int columns() const { return m_columns; }
    [[nodiscard]] int rows() const { return m_rows; }

    // Called with the new size in cells when the desktop gives one, before the program hears of it.
    std::function<void(int columns, int rows)> resized;

    std::optional<Event> poll_event()
    {
        if(m_events.empty())
        {
            read_messages();
        }
        if(m_events.empty())
        {
            return std::nullopt;
        }
        auto event{m_events.front()};
        m_events.pop_front();
        return event;
    }

    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
    {
        if(!m_events.empty())
        {
            return;
        }
        pollfd watched[2]{{m_socket.get(), POLLIN, 0}, {m_wake.get(), POLLIN, 0}};
        auto timeout{-1};
        if(deadline)
        {
            const auto left{
                std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())};
            timeout = static_cast<int>(std::max<std::chrono::milliseconds::rep>(0, left.count()));
        }
        ::poll(watched, 2, timeout);
        std::uint64_t woken{};
        if(::read(m_wake.get(), &woken, sizeof woken) < 0)
        {
            // Nothing waiting to be read is fine: the wait is over either way.
        }
    }

    void wake()
    {
        const std::uint64_t one{1};
        if(::write(m_wake.get(), &one, sizeof one) < 0)
        {
            // A wake-up already pending is enough, so a failed write needs no handling.
        }
    }

    [[nodiscard]] const std::string& clipboard_text() const { return m_clipboard; }

    void set_clipboard_text(std::string_view text)
    {
        m_clipboard = text;
        protocol::send_text(m_socket.get(), protocol::Message::clipboard, text);
    }

    void set_title(std::string_view title) { protocol::send(m_socket.get(), protocol::Message::title, title); }
    void set_pointer(Pointer pointer)
    {
        const auto byte{static_cast<char>(pointer)};
        protocol::send(m_socket.get(), protocol::Message::pointer, std::string_view{&byte, 1});
    }

    // A frame `width` by `height`, pixels or cells, `bytes` long: sent now, or once the desktop has taken the one
    // before, the last drawn by then.
    void show(const void* frame, std::size_t bytes, int width, int height)
    {
        if(bytes == 0)
        {
            return;
        }
        if(m_frame_out)
        {
            m_waiting.assign(static_cast<const char*>(frame), static_cast<const char*>(frame) + bytes);
            m_waiting_width = width;
            m_waiting_height = height;
            return;
        }
        send_frame(frame, bytes, width, height);
    }

  private:
    void read_messages()
    {
        protocol::Received received;
        while(!m_gone)
        {
            const auto result{protocol::receive(m_socket.get(), received)};
            if(result == protocol::Receive::nothing)
            {
                return;
            }
            if(result == protocol::Receive::gone)
            {
                m_gone = true;
                m_events.emplace_back(ShutdownEvent{ShutdownEvent::Reason::terminal_closed});
                return;
            }
            const Descriptor passed{received.passed};
            switch(received.type)
            {
            case protocol::Message::configure:
                if(const auto configure{protocol::payload_as<protocol::Configure>(received)})
                {
                    take(*configure);
                }
                break;
            case protocol::Message::taken:
                m_frame_out = false;
                if(!m_waiting.empty())
                {
                    const auto waiting{std::move(m_waiting)};
                    m_waiting.clear();
                    send_frame(waiting.data(), waiting.size(), m_waiting_width, m_waiting_height);
                }
                break;
            case protocol::Message::event:
                if(const auto wire{protocol::payload_as<protocol::WireEvent>(received)})
                {
                    if(const auto event{protocol::from_wire(*wire)})
                    {
                        m_events.push_back(*event);
                    }
                }
                break;
            case protocol::Message::close:
                m_events.emplace_back(CloseRequestEvent{});
                break;
            case protocol::Message::clipboard:
                if(auto text{protocol::text_of(received)})
                {
                    m_clipboard = std::move(*text);
                }
                break;
            default:
                break;
            }
        }
    }

    void take(const protocol::Configure& configure)
    {
        const auto columns{std::max(1, configure.columns)};
        const auto rows{std::max(1, configure.rows)};
        m_cells = configure.cells != 0;
        if(!m_configured || columns != m_columns || rows != m_rows)
        {
            m_columns = columns;
            m_rows = rows;
            if(resized)
            {
                resized(columns, rows);
            }
            m_events.emplace_back(ResizeEvent{static_cast<double>(columns), static_cast<double>(rows)});
        }
        if(!m_configured || (configure.focused != 0) != m_focused)
        {
            m_focused = configure.focused != 0;
            m_events.emplace_back(FocusEvent{m_focused});
        }
        m_configured = true;
    }

    // Into the shared memory, made again whenever the frame's size changes, and its news to the desktop.
    void send_frame(const void* frame, std::size_t bytes, int width, int height)
    {
        if(width != m_width || height != m_height || bytes != m_bytes)
        {
            unmap();
            Descriptor memory{::memfd_create("oui-window", MFD_CLOEXEC)};
            if(memory.get() < 0 || ::ftruncate(memory.get(), static_cast<off_t>(bytes)) != 0)
            {
                desktop_failure("shared memory");
            }
            auto* const map{::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memory.get(), 0)};
            if(map == MAP_FAILED)
            {
                desktop_failure("shared memory");
            }
            m_map = map;
            m_bytes = bytes;
            m_width = width;
            m_height = height;
            protocol::send_struct(m_socket.get(), protocol::Message::buffer, protocol::Buffer{m_width, m_height},
                                  memory.get());
        }
        std::memcpy(m_map, frame, bytes);
        protocol::send(m_socket.get(), protocol::Message::frame);
        m_frame_out = true;
    }

    void unmap()
    {
        if(m_map != nullptr)
        {
            ::munmap(m_map, m_bytes);
            m_map = nullptr;
        }
    }

    Descriptor m_socket;
    Descriptor m_wake;
    std::deque<Event> m_events;
    std::string m_clipboard;
    bool m_configured{false};
    bool m_cells{false};
    bool m_focused{false};
    bool m_gone{false};
    int m_columns{};
    int m_rows{};
    // A frame the desktop has not taken yet, and the last one drawn since, which waits for it to.
    bool m_frame_out{false};
    std::vector<char> m_waiting;
    int m_waiting_width{};
    int m_waiting_height{};
    void* m_map{};
    std::size_t m_bytes{};
    int m_width{};
    int m_height{};
};

// A window of cells on the desktop the link is to, which shows cells.
[[nodiscard]] std::unique_ptr<Backend> desktop_cell_backend(std::unique_ptr<DesktopLink> link);

} // namespace olagarro::oui
