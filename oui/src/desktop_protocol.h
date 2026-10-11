#pragma once

#include <oui/event.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

// What OUI's desktop and the programs on it say to each other, one message per packet of a local socket that keeps
// them apart (SOCK_SEQPACKET): a type, then what that type carries.
namespace olagarro::oui::desktop_protocol
{

enum class Message : std::uint32_t
{
    // Program to desktop: its title, as text, first of all.
    hello = 1,
    // Program to desktop: the memory its frames are in, passed along with it, `Buffer` big: 0x00RRGGBB pixels, or
    // `WireCell`s on a desktop of cells.
    buffer,
    // Program to desktop: a new frame is in the memory; it draws no other until the desktop has taken it.
    frame,
    // Program to desktop: a new title, as text.
    title,
    // Desktop to program: `Configure`, its size and whether it has the keys.
    configure,
    // Desktop to program: the frame was taken, the memory is the program's again.
    taken,
    // Desktop to program: a `WireEvent`.
    event,
    // Desktop to program: the person asked to close it.
    close,
    // Both ways: what was copied, in memory passed along, however long it is. A program sends what it copies;
    // the desktop keeps it and sends it to every other program, and to each new one.
    clipboard,
    // Program to desktop: the pointer it wants over its window, a Pointer as one byte.
    pointer
};

// How big a frame is: pixels across and down, or cells on a desktop of cells.
struct Buffer
{
    std::int32_t width;
    std::int32_t height;
};

struct Configure
{
    std::int32_t columns;
    std::int32_t rows;
    std::int32_t focused;
    // The desktop shows cells, in a terminal, so frames are cells instead of pixels.
    std::int32_t cells;
};

// A cell of a frame as it travels.
struct WireCell
{
    std::uint32_t glyph;
    std::uint8_t foreground[3];
    std::uint8_t background[3];
    std::uint8_t attributes;
    std::uint8_t continuation;
};
static_assert(sizeof(WireCell) == 12);

// An event as it travels: which one it is, and its fields in order.
struct WireEvent
{
    std::uint32_t kind;
    std::int32_t values[14];
};

// The largest message: a title is cut to fit.
inline constexpr std::size_t largest_message{4096};

[[nodiscard]] std::optional<WireEvent> to_wire(const Event& event);
[[nodiscard]] std::optional<Event> from_wire(const WireEvent& wire);

// Sends a message, with `passed` going along when it is a descriptor; false when the other side is gone.
bool send(int socket, Message type, std::string_view payload = {}, int passed = -1);

template <typename Payload> bool send_struct(int socket, Message type, const Payload& payload, int passed = -1)
{
    return send(socket, type, {reinterpret_cast<const char*>(&payload), sizeof payload}, passed);
}

// Sends `text` in memory of its own passed along: a message has room for little.
bool send_text(int socket, Message type, std::string_view text);

struct Received
{
    Message type;
    std::string payload;
    // A descriptor that came along, which the receiver then owns; -1 for none.
    int passed{-1};
};

enum class Receive
{
    message,
    nothing,
    gone
};

// The text in the memory a message passed along; nothing when it passed none.
[[nodiscard]] std::optional<std::string> text_of(const Received& received);

// Reads one message without waiting: `nothing` when none is there, `gone` when the other side closed.
[[nodiscard]] Receive receive(int socket, Received& received);

template <typename Payload> [[nodiscard]] std::optional<Payload> payload_as(const Received& received)
{
    if(received.payload.size() != sizeof(Payload))
    {
        return std::nullopt;
    }
    Payload payload;
    std::memcpy(&payload, received.payload.data(), sizeof payload);
    return payload;
}

} // namespace olagarro::oui::desktop_protocol
