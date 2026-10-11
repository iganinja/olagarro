#include "desktop_protocol.h"

#include <cerrno>
#include <cstring>
#include <type_traits>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace olagarro::oui::desktop_protocol
{

namespace
{

enum class Kind : std::uint32_t
{
    key = 1,
    text,
    mouse,
    focus,
    modifiers
};

} // namespace

std::optional<WireEvent> to_wire(const Event& event)
{
    WireEvent wire{};
    auto& values{wire.values};
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        wire.kind = static_cast<std::uint32_t>(Kind::key);
        values[0] = static_cast<std::int32_t>(key->key);
        values[1] = key->control;
        values[2] = key->alt;
        values[3] = key->shift;
        return wire;
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        wire.kind = static_cast<std::uint32_t>(Kind::text);
        values[0] = static_cast<std::int32_t>(text->codepoint);
        values[1] = text->control;
        values[2] = text->alt;
        values[3] = text->shift;
        return wire;
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        wire.kind = static_cast<std::uint32_t>(Kind::mouse);
        values[0] = mouse->x;
        values[1] = mouse->y;
        values[2] = mouse->pressed;
        values[3] = mouse->wheel_delta;
        values[4] = mouse->dragging;
        values[5] = mouse->released;
        values[6] = mouse->hover;
        values[7] = mouse->right;
        values[8] = mouse->shift;
        values[9] = mouse->control;
        values[10] = mouse->alt;
        values[11] = mouse->clicks;
        values[12] = mouse->cell_offset;
        return wire;
    }
    if(const auto* const focus{std::get_if<FocusEvent>(&event)})
    {
        wire.kind = static_cast<std::uint32_t>(Kind::focus);
        values[0] = focus->focused;
        return wire;
    }
    if(const auto* const modifiers{std::get_if<ModifiersEvent>(&event)})
    {
        wire.kind = static_cast<std::uint32_t>(Kind::modifiers);
        values[0] = modifiers->control;
        values[1] = modifiers->alt;
        values[2] = modifiers->shift;
        return wire;
    }
    return std::nullopt;
}

std::optional<Event> from_wire(const WireEvent& wire)
{
    const auto& values{wire.values};
    switch(static_cast<Kind>(wire.kind))
    {
    case Kind::key:
        if(values[0] < 0 || values[0] > static_cast<std::int32_t>(Key::f12))
        {
            return std::nullopt;
        }
        return KeyEvent{static_cast<Key>(values[0]), values[1] != 0, values[2] != 0, values[3] != 0};
    case Kind::text:
        return TextEvent{static_cast<char32_t>(values[0]), values[1] != 0, values[2] != 0, values[3] != 0};
    case Kind::mouse:
    {
        MouseEvent mouse{values[0], values[1], values[2] != 0};
        mouse.wheel_delta = values[3];
        mouse.dragging = values[4] != 0;
        mouse.released = values[5] != 0;
        mouse.hover = values[6] != 0;
        mouse.right = values[7] != 0;
        mouse.shift = values[8] != 0;
        mouse.control = values[9] != 0;
        mouse.alt = values[10] != 0;
        mouse.clicks = values[11];
        mouse.cell_offset = values[12];
        return mouse;
    }
    case Kind::focus:
        return FocusEvent{values[0] != 0};
    case Kind::modifiers:
        return ModifiersEvent{values[0] != 0, values[1] != 0, values[2] != 0};
    }
    return std::nullopt;
}

bool send(int socket, Message type, std::string_view payload, int passed)
{
    std::string packet(sizeof(Message), '\0');
    std::memcpy(packet.data(), &type, sizeof type);
    packet.append(payload.substr(0, largest_message - sizeof type));
    iovec part{packet.data(), packet.size()};
    msghdr message{};
    message.msg_iov = &part;
    message.msg_iovlen = 1;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    if(passed >= 0)
    {
        message.msg_control = control;
        message.msg_controllen = sizeof control;
        auto* const header{CMSG_FIRSTHDR(&message)};
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(header), &passed, sizeof passed);
    }
    while(true)
    {
        if(::sendmsg(socket, &message, MSG_NOSIGNAL) >= 0)
        {
            return true;
        }
        if(errno != EINTR)
        {
            return false;
        }
    }
}

bool send_text(int socket, Message type, std::string_view text)
{
    const auto memory{::memfd_create("oui-text", MFD_CLOEXEC)};
    if(memory < 0)
    {
        return false;
    }
    auto written{true};
    for(std::size_t done{}; done < text.size() && written;)
    {
        const auto count{::write(memory, text.data() + done, text.size() - done)};
        written = count > 0 || (count < 0 && errno == EINTR);
        done += count > 0 ? static_cast<std::size_t>(count) : 0;
    }
    const auto sent{written && send(socket, type, {}, memory)};
    ::close(memory);
    return sent;
}

std::optional<std::string> text_of(const Received& received)
{
    if(received.passed < 0)
    {
        return std::nullopt;
    }
    struct stat status{};
    if(::fstat(received.passed, &status) != 0)
    {
        return std::nullopt;
    }
    std::string text(static_cast<std::size_t>(status.st_size), '\0');
    std::size_t done{};
    while(done < text.size())
    {
        const auto count{::pread(received.passed, text.data() + done, text.size() - done, static_cast<off_t>(done))};
        if(count <= 0)
        {
            if(count < 0 && errno == EINTR)
            {
                continue;
            }
            return std::nullopt;
        }
        done += static_cast<std::size_t>(count);
    }
    return text;
}

Receive receive(int socket, Received& received)
{
    char buffer[largest_message];
    iovec part{buffer, sizeof buffer};
    msghdr message{};
    message.msg_iov = &part;
    message.msg_iovlen = 1;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    message.msg_control = control;
    message.msg_controllen = sizeof control;
    ssize_t count{};
    do
    {
        count = ::recvmsg(socket, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    } while(count < 0 && errno == EINTR);
    if(count < 0)
    {
        return errno == EAGAIN || errno == EWOULDBLOCK ? Receive::nothing : Receive::gone;
    }
    if(count == 0)
    {
        return Receive::gone;
    }
    received.passed = -1;
    // A message passes one descriptor at most, in its first and only control message.
    if(const auto* const header{CMSG_FIRSTHDR(&message)};
       header != nullptr && header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS)
    {
        std::memcpy(&received.passed, CMSG_DATA(header), sizeof received.passed);
    }
    if(static_cast<std::size_t>(count) < sizeof(Message))
    {
        if(received.passed >= 0)
        {
            ::close(received.passed);
        }
        return Receive::gone;
    }
    std::memcpy(&received.type, buffer, sizeof received.type);
    received.payload.assign(buffer + sizeof(Message), static_cast<std::size_t>(count) - sizeof(Message));
    return Receive::message;
}

} // namespace olagarro::oui::desktop_protocol
