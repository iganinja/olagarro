#include "keyboard_layout.h"

#include "../unicode_composition.h"

#include <linux/input-event-codes.h>

#include <utility>

namespace olagarro::oui
{

namespace
{

// The keys that write nothing and do something, the same on every layout.
[[nodiscard]] std::optional<Key> named_key(unsigned int code, bool shift)
{
    switch(code)
    {
    case KEY_TAB:
        return shift ? Key::shift_tab : Key::tab;
    case KEY_ENTER:
    case KEY_KPENTER:
        return Key::enter;
    case KEY_SPACE:
        return Key::space;
    case KEY_BACKSPACE:
        return Key::backspace;
    case KEY_DELETE:
        return Key::delete_;
    case KEY_ESC:
        return Key::escape;
    case KEY_LEFT:
        return Key::left;
    case KEY_RIGHT:
        return Key::right;
    case KEY_UP:
        return Key::up;
    case KEY_DOWN:
        return Key::down;
    case KEY_HOME:
        return Key::home;
    case KEY_END:
        return Key::end;
    case KEY_PAGEUP:
        return Key::page_up;
    case KEY_PAGEDOWN:
        return Key::page_down;
    case KEY_INSERT:
        return Key::insert_;
    case KEY_F1:
        return Key::f1;
    case KEY_F2:
        return Key::f2;
    case KEY_F3:
        return Key::f3;
    case KEY_F4:
        return Key::f4;
    case KEY_F5:
        return Key::f5;
    case KEY_F6:
        return Key::f6;
    case KEY_F7:
        return Key::f7;
    case KEY_F8:
        return Key::f8;
    case KEY_F9:
        return Key::f9;
    case KEY_F10:
        return Key::f10;
    case KEY_F11:
        return Key::f11;
    case KEY_F12:
        return Key::f12;
    default:
        return std::nullopt;
    }
}

// The places every layout shares: the letters that are where they always are, and the keypad's signs.
[[nodiscard]] std::vector<KeyboardLayout::Place> common_places()
{
    std::vector<KeyboardLayout::Place> places;
    constexpr std::pair<unsigned int, char32_t> letters[]{
        {KEY_A, U'a'}, {KEY_B, U'b'}, {KEY_C, U'c'}, {KEY_D, U'd'}, {KEY_E, U'e'}, {KEY_F, U'f'}, {KEY_G, U'g'},
        {KEY_H, U'h'}, {KEY_I, U'i'}, {KEY_J, U'j'}, {KEY_K, U'k'}, {KEY_L, U'l'}, {KEY_M, U'm'}, {KEY_N, U'n'},
        {KEY_O, U'o'}, {KEY_P, U'p'}, {KEY_Q, U'q'}, {KEY_R, U'r'}, {KEY_S, U's'}, {KEY_T, U't'}, {KEY_U, U'u'},
        {KEY_V, U'v'}, {KEY_W, U'w'}, {KEY_X, U'x'}, {KEY_Y, U'y'}, {KEY_Z, U'z'}};
    for(const auto& [code, letter] : letters)
    {
        places.push_back({code, letter, letter - U'a' + U'A', 0});
    }
    places.push_back({KEY_KPPLUS, U'+', U'+', 0});
    places.push_back({KEY_KPMINUS, U'-', U'-', 0});
    places.push_back({KEY_KPASTERISK, U'*', U'*', 0});
    places.push_back({KEY_KPSLASH, U'/', U'/', 0});
    return places;
}

[[nodiscard]] std::vector<KeyboardLayout::Place> us_places()
{
    auto places{common_places()};
    places.insert(places.end(), {
                                    {KEY_GRAVE, U'`', U'~', 0},       {KEY_1, U'1', U'!', 0},
                                    {KEY_2, U'2', U'@', 0},           {KEY_3, U'3', U'#', 0},
                                    {KEY_4, U'4', U'$', 0},           {KEY_5, U'5', U'%', 0},
                                    {KEY_6, U'6', U'^', 0},           {KEY_7, U'7', U'&', 0},
                                    {KEY_8, U'8', U'*', 0},           {KEY_9, U'9', U'(', 0},
                                    {KEY_0, U'0', U')', 0},           {KEY_MINUS, U'-', U'_', 0},
                                    {KEY_EQUAL, U'=', U'+', 0},       {KEY_LEFTBRACE, U'[', U'{', 0},
                                    {KEY_RIGHTBRACE, U']', U'}', 0},  {KEY_SEMICOLON, U';', U':', 0},
                                    {KEY_APOSTROPHE, U'\'', U'"', 0}, {KEY_BACKSLASH, U'\\', U'|', 0},
                                    {KEY_102ND, U'\\', U'|', 0},      {KEY_COMMA, U',', U'<', 0},
                                    {KEY_DOT, U'.', U'>', 0},         {KEY_SLASH, U'/', U'?', 0},
                                });
    return places;
}

// Spain's layout, as a PC keyboard sold there has it. The accents are dead keys: ´ and ¨ right of Ñ, ` and ^ right
// of P.
[[nodiscard]] std::vector<KeyboardLayout::Place> spanish_places()
{
    auto places{common_places()};
    for(auto& place : places)
    {
        if(place.code == KEY_E)
        {
            place.altgr = U'€';
        }
    }
    places.insert(places.end(), {
                                    {KEY_GRAVE, U'º', U'ª', U'\\'},
                                    {KEY_1, U'1', U'!', U'|'},
                                    {KEY_2, U'2', U'"', U'@'},
                                    {KEY_3, U'3', U'·', U'#'},
                                    {KEY_4, U'4', U'$', U'~'},
                                    {KEY_5, U'5', U'%', 0},
                                    {KEY_6, U'6', U'&', U'¬'},
                                    {KEY_7, U'7', U'/', 0},
                                    {KEY_8, U'8', U'(', 0},
                                    {KEY_9, U'9', U')', 0},
                                    {KEY_0, U'0', U'=', 0},
                                    {KEY_MINUS, U'\'', U'?', 0},
                                    {KEY_EQUAL, U'¡', U'¿', 0},
                                    {KEY_LEFTBRACE, U'̀', U'̂', U'['},
                                    {KEY_RIGHTBRACE, U'+', U'*', U']'},
                                    {KEY_SEMICOLON, U'ñ', U'Ñ', 0},
                                    {KEY_APOSTROPHE, U'́', U'̈', U'{'},
                                    {KEY_BACKSLASH, U'ç', U'Ç', U'}'},
                                    {KEY_102ND, U'<', U'>', 0},
                                    {KEY_COMMA, U',', U';', 0},
                                    {KEY_DOT, U'.', U':', 0},
                                    {KEY_SLASH, U'-', U'_', 0},
                                });
    return places;
}

// An accent as a character of its own, as a dead key writes it before a space.
[[nodiscard]] char32_t spacing(char32_t mark)
{
    switch(mark)
    {
    case U'̀':
        return U'`';
    case U'́':
        return U'´';
    case U'̂':
        return U'^';
    case U'̈':
        return U'¨';
    default:
        return mark;
    }
}

} // namespace

std::optional<KeyboardLayout> KeyboardLayout::named(std::string_view name)
{
    if(name == "us")
    {
        return KeyboardLayout{us_places(), false};
    }
    if(name == "es")
    {
        return KeyboardLayout{spanish_places(), true};
    }
    return std::nullopt;
}

KeyboardLayout::KeyboardLayout(std::vector<Place> places, bool right_alt_is_altgr)
    : m_places{std::move(places)}, m_right_alt_is_altgr{right_alt_is_altgr}
{
}

void KeyboardLayout::forget()
{
    m_left_control = m_right_control = m_left_shift = m_right_shift = m_left_alt = m_right_alt = false;
    m_dead.reset();
}

bool KeyboardLayout::control() const
{
    return m_left_control || m_right_control;
}

bool KeyboardLayout::alt() const
{
    return m_left_alt || (m_right_alt && !m_right_alt_is_altgr);
}

bool KeyboardLayout::shift() const
{
    return m_left_shift || m_right_shift;
}

const KeyboardLayout::Place* KeyboardLayout::place(unsigned int code) const
{
    for(const auto& each : m_places)
    {
        if(each.code == code)
        {
            return &each;
        }
    }
    return nullptr;
}

void KeyboardLayout::write(std::vector<Event>& events, char32_t character)
{
    events.push_back(TextEvent{character, false, false, false});
}

std::vector<Event> KeyboardLayout::key(unsigned int code, int value)
{
    std::vector<Event> events;
    const auto pressed{value != 0};
    bool* modifier{};
    switch(code)
    {
    case KEY_LEFTCTRL:
        modifier = &m_left_control;
        break;
    case KEY_RIGHTCTRL:
        modifier = &m_right_control;
        break;
    case KEY_LEFTSHIFT:
        modifier = &m_left_shift;
        break;
    case KEY_RIGHTSHIFT:
        modifier = &m_right_shift;
        break;
    case KEY_LEFTALT:
        modifier = &m_left_alt;
        break;
    case KEY_RIGHTALT:
        modifier = &m_right_alt;
        break;
    default:
        break;
    }
    if(modifier != nullptr)
    {
        const auto changed{*modifier != pressed};
        *modifier = pressed;
        if(changed)
        {
            events.push_back(ModifiersEvent{control(), alt(), shift()});
        }
        return events;
    }
    if(!pressed)
    {
        return events;
    }
    const auto altgr{m_right_alt && m_right_alt_is_altgr};
    // A dead key's accent and a space: the accent on its own.
    if(m_dead && code == KEY_SPACE && !control() && !alt())
    {
        write(events, spacing(*std::exchange(m_dead, std::nullopt)));
        return events;
    }
    if(const auto named{named_key(code, shift())})
    {
        m_dead.reset();
        events.push_back(KeyEvent{*named, control(), alt(), shift() && *named != Key::shift_tab});
        return events;
    }
    const auto* const found{place(code)};
    if(found == nullptr)
    {
        return events;
    }
    // With Ctrl or Alt a key writes nothing: it is a shortcut, with the character the key makes with Shift as it
    // is, lower case for a letter, as a terminal sends it; a dead key gives its accent on its own.
    if(control() || alt())
    {
        auto character{shift() ? found->shifted : found->plain};
        if(character >= U'A' && character <= U'Z')
        {
            character = character - U'A' + U'a';
        }
        character = is_diacritic(character) ? spacing(character) : character;
        if(character != 0)
        {
            events.push_back(TextEvent{character, control(), alt(), shift()});
        }
        return events;
    }
    const auto character{altgr ? found->altgr : shift() ? found->shifted : found->plain};
    if(character == 0)
    {
        return events;
    }
    if(is_diacritic(character))
    {
        // Two dead keys: the first accent on its own, the second waits.
        if(m_dead)
        {
            write(events, spacing(*m_dead));
        }
        m_dead = character;
        return events;
    }
    if(m_dead)
    {
        const auto mark{*std::exchange(m_dead, std::nullopt)};
        if(const auto accented{compose(character, mark)})
        {
            write(events, *accented);
            return events;
        }
        // A letter the accent does not go on: both, as typed.
        write(events, spacing(mark));
    }
    events.push_back(TextEvent{character, false, false, character >= U'A' && character <= U'Z'});
    return events;
}

} // namespace olagarro::oui
