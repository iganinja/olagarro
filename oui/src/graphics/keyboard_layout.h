#pragma once

#include <oui/event.h>

#include <optional>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// Turns the keys a Linux keyboard reports, by their place on the board (the kernel's event codes), into OUI's key,
// text and modifier events, by a layout: what each place writes, alone, with Shift and with AltGr, and the dead keys
// that put an accent on the letter after them. The layouts are built in, so one holds the same on every machine,
// with nothing to install or set up beside the program.
class KeyboardLayout
{
  public:
    // "us" or "es"; nothing for a layout it does not know.
    [[nodiscard]] static std::optional<KeyboardLayout> named(std::string_view name);

    // A key went down (1), repeated (2) or came up (0): what the application hears of it, if anything.
    [[nodiscard]] std::vector<Event> key(unsigned int code, int value);

    // Forgets the keys held and a dead key's accent, as when the keyboard went to another program, which saw them
    // let go.
    void forget();

    // The modifiers held now, as a pointer's events carry them.
    [[nodiscard]] bool control() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool shift() const;

    // What a place on the board writes: alone, with Shift and with AltGr; 0 where it writes nothing. A combining
    // mark (U+0300 to U+036F) is a dead key: it waits for the next letter and accents it.
    struct Place
    {
        unsigned int code;
        char32_t plain;
        char32_t shifted;
        char32_t altgr;
    };

  private:
    explicit KeyboardLayout(std::vector<Place> places, bool right_alt_is_altgr);

    [[nodiscard]] const Place* place(unsigned int code) const;
    // The character a dead key's accent is on its own, as it is written after a space.
    void write(std::vector<Event>& events, char32_t character);

    std::vector<Place> m_places;
    // On a layout with AltGr, the right Alt is AltGr; on one without, it is Alt like the left.
    bool m_right_alt_is_altgr;
    bool m_left_control{false};
    bool m_right_control{false};
    bool m_left_shift{false};
    bool m_right_shift{false};
    bool m_left_alt{false};
    bool m_right_alt{false};
    // The accent a dead key left for the next letter.
    std::optional<char32_t> m_dead;
};

} // namespace olagarro::oui
