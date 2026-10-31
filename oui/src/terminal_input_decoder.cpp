#include "terminal_input_decoder.h"

#include <oui/utf8.h>

#include <vector>

namespace olagarro::oui
{

namespace
{

constexpr char escape{'\x1b'};

// Longest escape sequence accepted before the bytes are discarded as garbage.
constexpr std::size_t max_sequence_length{64};

struct Modifiers
{
    bool control{false};
    bool alt{false};
    bool shift{false};
};

// xterm encodes modifiers as 1 + (shift | alt << 1 | control << 2).
Modifiers decode_modifiers(int parameter)
{
    const auto bits{parameter > 1 ? parameter - 1 : 0};
    return {(bits & 4) != 0, (bits & 2) != 0, (bits & 1) != 0};
}

KeyEvent key(Key value, Modifiers modifiers)
{
    return {value, modifiers.control, modifiers.alt, modifiers.shift};
}

std::size_t utf8_length(unsigned char lead)
{
    if(lead >= 0xc2 && lead <= 0xdf)
    {
        return 2;
    }
    if(lead >= 0xe0 && lead <= 0xef)
    {
        return 3;
    }
    if(lead >= 0xf0 && lead <= 0xf4)
    {
        return 4;
    }
    return 1;
}

std::vector<int> parse_parameters(std::string_view text)
{
    std::vector<int> parameters{0};
    for(const auto character : text)
    {
        if(character >= '0' && character <= '9')
        {
            parameters.back() = parameters.back() * 10 + (character - '0');
        }
        else if(character == ';')
        {
            parameters.push_back(0);
        }
    }
    return parameters;
}

int parameter_or(const std::vector<int>& parameters, std::size_t index, int fallback)
{
    return index < parameters.size() && parameters.at(index) != 0 ? parameters.at(index) : fallback;
}

// A key reported by codepoint, as modifyOtherKeys (CSI 27;m;c ~) and CSI u do.
std::optional<Event> codepoint_event(char32_t codepoint, Modifiers modifiers)
{
    switch(codepoint)
    {
    case 9:
        return key(modifiers.shift ? Key::shift_tab : Key::tab, modifiers);
    case 13:
        return key(Key::enter, modifiers);
    case 27:
        return key(Key::escape, modifiers);
    case 8:
    case 127:
        return key(Key::backspace, modifiers);
    case 32:
        return key(Key::space, modifiers);
    default:
        break;
    }
    if(codepoint < 0x20)
    {
        return std::nullopt;
    }
    auto character{codepoint};
    // Shortcuts are bound to lowercase letters; Shift travels in the modifier.
    if((modifiers.control || modifiers.alt) && character >= U'A' && character <= U'Z')
    {
        character = character - U'A' + U'a';
    }
    return TextEvent{character, modifiers.control, modifiers.alt, modifiers.shift};
}

std::optional<Event> unmodified_mouse_event(const std::vector<int>& parameters, bool release)
{
    if(parameters.size() < 3)
    {
        return std::nullopt;
    }
    const auto button{parameters.at(0)};
    const double x{static_cast<double>(parameters.at(1) - 1)};
    const double y{static_cast<double>(parameters.at(2) - 1)};
    if((button & 64) != 0)
    {
        if((button & 3) == 0)
        {
            return MouseEvent{x, y, false, 1};
        }
        if((button & 3) == 1)
        {
            return MouseEvent{x, y, false, -1};
        }
        return std::nullopt;
    }
    if(release)
    {
        return MouseEvent{x, y, false, 0, false, true};
    }
    if((button & 32) != 0)
    {
        if((button & 3) == 0)
        {
            return MouseEvent{x, y, false, 0, true};
        }
        if((button & 3) == 3)
        {
            return MouseEvent{x, y, false, 0, false, false, true};
        }
        return std::nullopt;
    }
    if((button & 3) == 0)
    {
        return MouseEvent{x, y, true};
    }
    if((button & 3) == 2)
    {
        return MouseEvent{x, y, true, 0, false, false, false, true};
    }
    return std::nullopt;
}

// SGR mouse reports carry Shift, Alt and Ctrl in bits 2, 3 and 4 of the button number.
std::optional<Event> mouse_event(const std::vector<int>& parameters, bool release)
{
    auto event{unmodified_mouse_event(parameters, release)};
    if(auto* const mouse{event ? std::get_if<MouseEvent>(&*event) : nullptr})
    {
        const auto button{parameters.at(0)};
        mouse->shift = (button & 4) != 0;
        mouse->alt = (button & 8) != 0;
        mouse->control = (button & 16) != 0;
    }
    return event;
}

std::optional<Event> with_alt(std::optional<Event> event)
{
    if(!event)
    {
        return event;
    }
    if(auto* const text{std::get_if<TextEvent>(&*event)})
    {
        text->alt = true;
    }
    else if(auto* const key_event{std::get_if<KeyEvent>(&*event)})
    {
        key_event->alt = true;
    }
    return event;
}

} // namespace

void TerminalInputDecoder::feed(std::string_view bytes)
{
    m_buffer.append(bytes);
}

std::optional<Event> TerminalInputDecoder::next()
{
    while(!m_buffer.empty())
    {
        const auto parsed{parse(m_buffer, m_final)};
        if(parsed.consumed == 0)
        {
            return std::nullopt;
        }
        m_buffer.erase(0, parsed.consumed);
        if(m_buffer.empty())
        {
            m_final = false;
        }
        if(parsed.event)
        {
            return parsed.event;
        }
    }
    return std::nullopt;
}

bool TerminalInputDecoder::has_pending() const
{
    return !m_buffer.empty();
}

void TerminalInputDecoder::flush()
{
    m_final = true;
}

TerminalInputDecoder::Parsed TerminalInputDecoder::parse(std::string_view bytes, bool final) const
{
    if(bytes.front() == escape)
    {
        return parse_escape(bytes, final);
    }
    return parse_plain(bytes, final);
}

TerminalInputDecoder::Parsed TerminalInputDecoder::parse_escape(std::string_view bytes, bool final) const
{
    if(bytes.size() == 1)
    {
        return final ? Parsed{1, KeyEvent{Key::escape}} : Parsed{};
    }
    if(bytes.at(1) == '[')
    {
        return parse_csi(bytes, final);
    }
    if(bytes.at(1) == 'O')
    {
        return parse_ss3(bytes, final);
    }
    if(bytes.at(1) == escape)
    {
        return {1, KeyEvent{Key::escape}};
    }
    // ESC followed by a key is how terminals report Alt+key.
    const auto rest{parse_plain(bytes.substr(1), final)};
    if(rest.consumed == 0)
    {
        return {};
    }
    return {rest.consumed + 1, with_alt(rest.event)};
}

TerminalInputDecoder::Parsed TerminalInputDecoder::parse_csi(std::string_view bytes, bool final) const
{
    std::size_t end{2};
    while(end < bytes.size() && (bytes.at(end) < 0x40 || bytes.at(end) > 0x7e))
    {
        ++ end;
    }
    if(end == bytes.size())
    {
        if(bytes.size() > max_sequence_length)
        {
            return {bytes.size(), std::nullopt};
        }
        // Without its final byte, "ESC [" was Alt+[ typed by the user.
        return final ? Parsed{2, TextEvent{U'[', false, true}} : Parsed{};
    }
    const auto consumed{end + 1};
    const auto body{bytes.substr(2, end - 2)};
    const auto terminator{bytes.at(end)};
    const auto parameters{parse_parameters(body)};
    if(!body.empty() && body.front() == '<')
    {
        if(terminator == 'M' || terminator == 'm')
        {
            return {consumed, mouse_event(parameters, terminator == 'm')};
        }
        return {consumed, std::nullopt};
    }
    const auto modifiers{decode_modifiers(parameter_or(parameters, 1, 1))};
    switch(terminator)
    {
    case 'A':
        return {consumed, key(Key::up, modifiers)};
    case 'B':
        return {consumed, key(Key::down, modifiers)};
    case 'C':
        return {consumed, key(Key::right, modifiers)};
    case 'D':
        return {consumed, key(Key::left, modifiers)};
    case 'H':
        return {consumed, key(Key::home, modifiers)};
    case 'F':
        return {consumed, key(Key::end, modifiers)};
    case 'P':
        return {consumed, key(Key::f1, modifiers)};
    case 'Q':
        return {consumed, key(Key::f2, modifiers)};
    case 'R':
        return {consumed, key(Key::f3, modifiers)};
    case 'S':
        return {consumed, key(Key::f4, modifiers)};
    case 'I':
        return {consumed, FocusEvent{true}};
    case 'O':
        return {consumed, FocusEvent{false}};
    case 'Z':
        return {consumed, KeyEvent{Key::shift_tab, false, false, true}};
    case 'u':
        return {consumed, codepoint_event(static_cast<char32_t>(parameter_or(parameters, 0, 0)), modifiers)};
    case '~':
        break;
    default:
        return {consumed, std::nullopt};
    }
    switch(parameter_or(parameters, 0, 0))
    {
    case 1:
    case 7:
        return {consumed, key(Key::home, modifiers)};
    case 2:
        return {consumed, key(Key::insert_, modifiers)};
    case 3:
        return {consumed, key(Key::delete_, modifiers)};
    case 4:
    case 8:
        return {consumed, key(Key::end, modifiers)};
    case 5:
        return {consumed, key(Key::page_up, modifiers)};
    case 6:
        return {consumed, key(Key::page_down, modifiers)};
    case 11:
        return {consumed, key(Key::f1, modifiers)};
    case 12:
        return {consumed, key(Key::f2, modifiers)};
    case 13:
        return {consumed, key(Key::f3, modifiers)};
    case 14:
        return {consumed, key(Key::f4, modifiers)};
    case 15:
        return {consumed, key(Key::f5, modifiers)};
    case 17:
        return {consumed, key(Key::f6, modifiers)};
    case 18:
        return {consumed, key(Key::f7, modifiers)};
    case 19:
        return {consumed, key(Key::f8, modifiers)};
    case 20:
        return {consumed, key(Key::f9, modifiers)};
    case 21:
        return {consumed, key(Key::f10, modifiers)};
    case 23:
        return {consumed, key(Key::f11, modifiers)};
    case 24:
        return {consumed, key(Key::f12, modifiers)};
    case 27:
        return {consumed, codepoint_event(static_cast<char32_t>(parameter_or(parameters, 2, 0)), modifiers)};
    default:
        return {consumed, std::nullopt};
    }
}

TerminalInputDecoder::Parsed TerminalInputDecoder::parse_ss3(std::string_view bytes, bool final)
{
    if(bytes.size() < 3)
    {
        return final ? Parsed{2, TextEvent{U'O', false, true, true}} : Parsed{};
    }
    // Konsole's form for a key with a modifier: the modifier before the letter, ESC O 2 S for Shift+F4.
    if(bytes.at(2) >= '2' && bytes.at(2) <= '9')
    {
        if(bytes.size() < 4)
        {
            return final ? Parsed{2, TextEvent{U'O', false, true, true}} : Parsed{};
        }
        const auto modifiers{decode_modifiers(bytes.at(2) - '0')};
        switch(bytes.at(3))
        {
        case 'A':
            return {4, key(Key::up, modifiers)};
        case 'B':
            return {4, key(Key::down, modifiers)};
        case 'C':
            return {4, key(Key::right, modifiers)};
        case 'D':
            return {4, key(Key::left, modifiers)};
        case 'H':
            return {4, key(Key::home, modifiers)};
        case 'F':
            return {4, key(Key::end, modifiers)};
        case 'P':
            return {4, key(Key::f1, modifiers)};
        case 'Q':
            return {4, key(Key::f2, modifiers)};
        case 'R':
            return {4, key(Key::f3, modifiers)};
        case 'S':
            return {4, key(Key::f4, modifiers)};
        default:
            return {4, std::nullopt};
        }
    }
    switch(bytes.at(2))
    {
    case 'A':
        return {3, KeyEvent{Key::up}};
    case 'B':
        return {3, KeyEvent{Key::down}};
    case 'C':
        return {3, KeyEvent{Key::right}};
    case 'D':
        return {3, KeyEvent{Key::left}};
    case 'H':
        return {3, KeyEvent{Key::home}};
    case 'F':
        return {3, KeyEvent{Key::end}};
    case 'M':
        return {3, KeyEvent{Key::enter}};
    case 'P':
        return {3, KeyEvent{Key::f1}};
    case 'Q':
        return {3, KeyEvent{Key::f2}};
    case 'R':
        return {3, KeyEvent{Key::f3}};
    case 'S':
        return {3, KeyEvent{Key::f4}};
    default:
        return {3, std::nullopt};
    }
}

TerminalInputDecoder::Parsed TerminalInputDecoder::parse_plain(std::string_view bytes, bool final)
{
    const auto byte{static_cast<unsigned char>(bytes.front())};
    switch(byte)
    {
    case 0x00:
        return {1, KeyEvent{Key::space, true}};
    case '\t':
        return {1, KeyEvent{Key::tab}};
    case '\r':
    case '\n':
        return {1, KeyEvent{Key::enter}};
    case 0x7f:
        return {1, KeyEvent{Key::backspace}};
    case ' ':
        return {1, KeyEvent{Key::space}};
    default:
        break;
    }
    // Ctrl+letter arrives as 0x01..0x1a. Backspace sends 0x7f, so 0x08 is Ctrl+H.
    if(byte <= 0x1a)
    {
        return {1, TextEvent{static_cast<char32_t>(U'a' + byte - 1), true}};
    }
    // Terminals send Ctrl+/ as 0x1f, the same byte as Ctrl+_; Ctrl+\ and Ctrl+] are 0x1c and 0x1d.
    if(byte == 0x1f)
    {
        return {1, TextEvent{U'/', true}};
    }
    if(byte == 0x1c || byte == 0x1d)
    {
        return {1, TextEvent{byte == 0x1c ? U'\\' : U']', true}};
    }
    if(byte < 0x20)
    {
        return {1, std::nullopt};
    }
    if(byte < 0x80)
    {
        return {1, TextEvent{byte, false, false, byte >= 'A' && byte <= 'Z'}};
    }
    const auto length{utf8_length(byte)};
    if(bytes.size() < length)
    {
        return final ? Parsed{1, std::nullopt} : Parsed{};
    }
    std::size_t index{};
    const auto codepoint{decode_utf8(bytes.substr(0, length), index)};
    return {index, TextEvent{codepoint}};
}

} // namespace olagarro::oui
