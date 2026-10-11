#pragma once

#include <oui/event.h>

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

// Turns the byte stream a VT/xterm-compatible terminal sends into OUI events.
//
// Bytes may arrive split across reads, so an incomplete escape or UTF-8 sequence stays buffered
// until more bytes complete it. A lone ESC is ambiguous (Escape key or the start of a sequence):
// the caller decides when enough time has passed and calls flush() to resolve it as a key.
class TerminalInputDecoder
{
  public:
    void feed(std::string_view bytes);

    // The next complete event, or nothing when the buffer is empty or holds only an incomplete sequence.
    [[nodiscard]] std::optional<Event> next();

    // True when buffered bytes are waiting for the rest of a sequence.
    [[nodiscard]] bool has_pending() const;

    // Resolves buffered incomplete sequences as if no more bytes will follow.
    void flush();

  private:
    struct Parsed
    {
        std::size_t consumed{};
        std::optional<Event> event;
    };

    [[nodiscard]] Parsed parse(std::string_view bytes, bool final) const;
    [[nodiscard]] Parsed parse_escape(std::string_view bytes, bool final) const;
    [[nodiscard]] Parsed parse_csi(std::string_view bytes, bool final) const;
    [[nodiscard]] static Parsed parse_ss3(std::string_view bytes, bool final);
    [[nodiscard]] static Parsed parse_plain(std::string_view bytes, bool final);

    std::string m_buffer;
    bool m_final{false};
};

} // namespace olagarro::oui
