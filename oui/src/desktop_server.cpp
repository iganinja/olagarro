#include <oui/desktop.h>

#include "descriptor.h"
#include "desktop_protocol.h"

#include <oui/application.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace olagarro::oui
{

namespace
{

namespace protocol = desktop_protocol;

[[noreturn]] void fail(const std::string& what)
{
    throw std::system_error{errno, std::generic_category(), "Cannot start the desktop: " + what};
}

[[nodiscard]] sockaddr_un address_of(const std::filesystem::path& socket)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto path{socket.string()};
    if(path.size() >= sizeof address.sun_path)
    {
        errno = ENAMETOOLONG;
        fail(path);
    }
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return address;
}

// A program's end of the conversation. The socket is the application thread's to write to and the reader's to read
// from; the shared memory is the reader's alone.
struct Connection
{
    explicit Connection(int id, int socket) : id{id}, socket{socket} {}
    ~Connection()
    {
        if(map != nullptr)
        {
            ::munmap(const_cast<std::uint32_t*>(map), bytes);
        }
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int id;
    Descriptor socket;
    const std::uint32_t* map{};
    std::size_t bytes{};
    int width{};
    int height{};
};

} // namespace

class DesktopServer::Impl
{
  public:
    Impl(DesktopServer& owner, Application& application, std::filesystem::path socket, DesktopFrames frames)
        : m_owner{owner}, m_application{application}, m_path{std::move(socket)}, m_frames{frames}
    {
        const auto address{address_of(m_path)};
        m_listener = Descriptor{::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)};
        if(m_listener.get() < 0)
        {
            fail("socket");
        }
        if(::bind(m_listener.get(), reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            fail(m_path.string());
        }
        ::chmod(m_path.c_str(), 0600);
        if(::listen(m_listener.get(), 16) != 0)
        {
            fail("listen");
        }
        m_wake = Descriptor{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
        m_reader = std::thread{[this] { read(); }};
    }

    ~Impl()
    {
        m_alive->store(false);
        m_stopping = true;
        const std::uint64_t one{1};
        if(::write(m_wake.get(), &one, sizeof one) < 0)
        {
            // A wake-up already pending is enough, so a failed write needs no handling.
        }
        m_reader.join();
        ::unlink(m_path.c_str());
    }

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    [[nodiscard]] const Client* client(int id) const
    {
        const auto found{m_clients.find(id)};
        return found == m_clients.end() ? nullptr : &found->second.client;
    }

    void configure(int id, int columns, int rows, bool focused)
    {
        if(const auto* const connection{connection_of(id)})
        {
            protocol::send_struct(
                connection->socket.get(), protocol::Message::configure,
                protocol::Configure{columns, rows, focused ? 1 : 0, m_frames == DesktopFrames::cells ? 1 : 0});
        }
    }

    void send(int id, const Event& event)
    {
        const auto* const connection{connection_of(id)};
        const auto wire{protocol::to_wire(event)};
        if(connection != nullptr && wire)
        {
            protocol::send_struct(connection->socket.get(), protocol::Message::event, *wire);
        }
    }

    void ask_to_close(int id)
    {
        if(const auto* const connection{connection_of(id)})
        {
            protocol::send(connection->socket.get(), protocol::Message::close);
        }
    }

  private:
    // What the application thread knows of a program.
    struct Known
    {
        Client client;
        std::shared_ptr<Connection> connection;
    };

    [[nodiscard]] const Connection* connection_of(int id) const
    {
        const auto found{m_clients.find(id)};
        return found == m_clients.end() ? nullptr : found->second.connection.get();
    }

    // Runs `work` on the application thread, unless the server is gone by then.
    template <typename Work> void on_application(Work work)
    {
        m_application.post(
            [alive{m_alive}, work{std::move(work)}]() mutable
            {
                if(alive->load())
                {
                    work();
                }
            });
    }

    // The reader: takes new programs and what they send, until the server goes.
    void read()
    {
        std::vector<std::shared_ptr<Connection>> connections;
        auto next_id{1};
        while(!m_stopping.load())
        {
            std::vector<pollfd> watched{{m_listener.get(), POLLIN, 0}, {m_wake.get(), POLLIN, 0}};
            for(const auto& connection : connections)
            {
                watched.push_back({connection->socket.get(), POLLIN, 0});
            }
            if(::poll(watched.data(), watched.size(), -1) < 0 && errno != EINTR)
            {
                return;
            }
            if((watched[0].revents & POLLIN) != 0)
            {
                while(true)
                {
                    const auto accepted{::accept4(m_listener.get(), nullptr, nullptr, SOCK_CLOEXEC)};
                    if(accepted < 0)
                    {
                        break;
                    }
                    connections.push_back(std::make_shared<Connection>(next_id++, accepted));
                }
            }
            std::uint64_t woken{};
            if(::read(m_wake.get(), &woken, sizeof woken) < 0)
            {
                // Nothing waiting to be read is fine: the wait is over either way.
            }
            for(std::size_t index{2}; index < watched.size(); ++ index)
            {
                if(watched[index].revents == 0)
                {
                    continue;
                }
                auto& connection{connections[index - 2]};
                if(!take_messages(connection))
                {
                    on_application(
                        [this, id{connection->id}]
                        {
                            if(m_clients.erase(id) > 0)
                            {
                                m_owner.m_disconnected.fire(id);
                            }
                        });
                    connection.reset();
                }
            }
            connections.erase(std::remove(connections.begin(), connections.end(), nullptr), connections.end());
        }
    }

    // Everything the program sent so far; false once it is gone or says what makes no sense.
    bool take_messages(const std::shared_ptr<Connection>& connection)
    {
        protocol::Received received;
        while(true)
        {
            const auto result{protocol::receive(connection->socket.get(), received)};
            if(result == protocol::Receive::nothing)
            {
                return true;
            }
            if(result == protocol::Receive::gone)
            {
                return false;
            }
            Descriptor passed{received.passed};
            switch(received.type)
            {
            case protocol::Message::hello:
                on_application(
                    [this, connection, title{std::move(received.payload)}]() mutable
                    {
                        const auto id{connection->id};
                        if(!m_clipboard.empty())
                        {
                            protocol::send_text(connection->socket.get(), protocol::Message::clipboard, m_clipboard);
                        }
                        m_clients[id] = Known{Client{id, std::move(title), {}, {}, 0, 0}, std::move(connection)};
                        m_owner.m_connected.fire(id);
                    });
                break;
            case protocol::Message::title:
                on_application(
                    [this, id{connection->id}, title{std::move(received.payload)}]() mutable
                    {
                        if(const auto found{m_clients.find(id)}; found != m_clients.end())
                        {
                            found->second.client.title = std::move(title);
                            m_owner.m_changed.fire(id);
                        }
                    });
                break;
            case protocol::Message::pointer:
            {
                // One byte, a Pointer there is; anything else is not a program of ours.
                if(received.payload.size() != 1 || static_cast<unsigned char>(received.payload.front()) >
                                                       static_cast<unsigned char>(Pointer::resize_rows))
                {
                    return false;
                }
                const auto pointer{static_cast<Pointer>(received.payload.front())};
                on_application(
                    [this, id{connection->id}, pointer]
                    {
                        if(const auto found{m_clients.find(id)}; found != m_clients.end())
                        {
                            found->second.client.pointer = pointer;
                            m_owner.m_pointer.fire(id);
                        }
                    });
                break;
            }
            case protocol::Message::clipboard:
            {
                auto text{protocol::text_of(received)};
                if(!text)
                {
                    return false;
                }
                on_application(
                    [this, id{connection->id}, text{std::move(*text)}]() mutable
                    {
                        m_clipboard = std::move(text);
                        for(const auto& [other, known] : m_clients)
                        {
                            if(other != id)
                            {
                                protocol::send_text(known.connection->socket.get(), protocol::Message::clipboard,
                                                    m_clipboard);
                            }
                        }
                    });
                break;
            }
            case protocol::Message::buffer:
            {
                const auto size{protocol::payload_as<protocol::Buffer>(received)};
                if(!size || passed.get() < 0 || size->width <= 0 || size->height <= 0)
                {
                    return false;
                }
                const auto unit{m_frames == DesktopFrames::cells ? sizeof(protocol::WireCell) : sizeof(std::uint32_t)};
                const auto bytes{static_cast<std::size_t>(size->width) * static_cast<std::size_t>(size->height) * unit};
                struct stat status
                {
                };
                if(::fstat(passed.get(), &status) != 0 || static_cast<std::size_t>(status.st_size) < bytes)
                {
                    return false;
                }
                auto* const map{::mmap(nullptr, bytes, PROT_READ, MAP_SHARED, passed.get(), 0)};
                if(map == MAP_FAILED)
                {
                    return false;
                }
                if(connection->map != nullptr)
                {
                    ::munmap(const_cast<std::uint32_t*>(connection->map), connection->bytes);
                }
                connection->map = static_cast<const std::uint32_t*>(map);
                connection->bytes = bytes;
                connection->width = size->width;
                connection->height = size->height;
                break;
            }
            case protocol::Message::frame:
            {
                if(connection->map == nullptr)
                {
                    return false;
                }
                // A copy of its own, so the program may draw the next frame while the desktop shows this one.
                std::vector<std::uint32_t> pixels;
                std::vector<Cell> cells;
                if(m_frames == DesktopFrames::cells)
                {
                    const auto* const wire{reinterpret_cast<const protocol::WireCell*>(connection->map)};
                    const auto count{connection->bytes / sizeof(protocol::WireCell)};
                    cells.reserve(count);
                    for(std::size_t index{}; index < count; ++ index)
                    {
                        const auto& each{wire[index]};
                        cells.push_back({static_cast<char32_t>(each.glyph),
                                         {each.foreground[0], each.foreground[1], each.foreground[2]},
                                         {each.background[0], each.background[1], each.background[2]},
                                         each.attributes,
                                         each.continuation != 0});
                    }
                }
                else
                {
                    pixels.assign(connection->map, connection->map + connection->bytes / sizeof(std::uint32_t));
                }
                protocol::send(connection->socket.get(), protocol::Message::taken);
                on_application(
                    [this, id{connection->id}, width{connection->width}, height{connection->height},
                     pixels{std::move(pixels)}, cells{std::move(cells)}]() mutable
                    {
                        if(const auto found{m_clients.find(id)}; found != m_clients.end())
                        {
                            auto& client{found->second.client};
                            client.pixels = std::move(pixels);
                            client.cells = std::move(cells);
                            client.width = width;
                            client.height = height;
                            m_owner.m_changed.fire(id);
                        }
                    });
                break;
            }
            default:
                return false;
            }
        }
    }

    DesktopServer& m_owner;
    Application& m_application;
    std::filesystem::path m_path;
    DesktopFrames m_frames;
    Descriptor m_listener;
    Descriptor m_wake;
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    std::atomic<bool> m_stopping{false};
    std::map<int, Known> m_clients;
    // What was copied last, in any program.
    std::string m_clipboard;
    std::thread m_reader;
};

DesktopServer::DesktopServer(Application& application, std::filesystem::path socket, DesktopFrames frames)
    : m_impl{std::make_unique<Impl>(*this, application, std::move(socket), frames)}
{
}

DesktopServer::~DesktopServer() = default;

const std::filesystem::path& DesktopServer::socket() const
{
    return m_impl->path();
}

const DesktopServer::Client* DesktopServer::client(int id) const
{
    return m_impl->client(id);
}

void DesktopServer::configure(int id, int columns, int rows, bool focused)
{
    m_impl->configure(id, columns, rows, focused);
}

void DesktopServer::send(int id, const Event& event)
{
    m_impl->send(id, event);
}

void DesktopServer::ask_to_close(int id)
{
    m_impl->ask_to_close(id);
}

} // namespace olagarro::oui
