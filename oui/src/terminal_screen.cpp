#include <oui/terminal_screen.h>

#include <oui/utf8.h>

#include "unicode_composition.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace olagarro::oui
{

namespace
{

// The 256 colours a program can name: the 16 of the PC's text screen, a cube of 216 and 24 greys, as xterm has
// them.
[[nodiscard]] Color palette(int index)
{
    static constexpr std::array<Color, 16> base{{{0, 0, 0},
                                                 {170, 0, 0},
                                                 {0, 170, 0},
                                                 {170, 85, 0},
                                                 {0, 0, 170},
                                                 {170, 0, 170},
                                                 {0, 170, 170},
                                                 {170, 170, 170},
                                                 {85, 85, 85},
                                                 {255, 85, 85},
                                                 {85, 255, 85},
                                                 {255, 255, 85},
                                                 {85, 85, 255},
                                                 {255, 85, 255},
                                                 {85, 255, 255},
                                                 {255, 255, 255}}};
    index = std::clamp(index, 0, 255);
    if(index < 16)
    {
        return base[static_cast<std::size_t>(index)];
    }
    if(index < 232)
    {
        const auto cube{index - 16};
        const auto level{[](int value) { return static_cast<std::uint8_t>(value == 0 ? 0 : 55 + value * 40); }};
        return {level(cube / 36), level((cube / 6) % 6), level(cube % 6)};
    }
    const auto grey{static_cast<std::uint8_t>(8 + (index - 232) * 10)};
    return {grey, grey, grey};
}

[[nodiscard]] TerminalColor named(int index)
{
    return {false, palette(index), index < 8 ? index : -1};
}

constexpr std::size_t history_limit{5000};

} // namespace

TerminalScreen::TerminalScreen(int columns, int rows)
    : m_columns{std::max(1, columns)}, m_rows{std::max(1, rows)}, m_bottom{m_rows - 1}
{
    m_lines.assign(static_cast<std::size_t>(m_rows), blank_line());
    m_other_lines = m_lines;
}

int TerminalScreen::columns() const
{
    return m_columns;
}

int TerminalScreen::rows() const
{
    return m_rows;
}

int TerminalScreen::history_size() const
{
    return m_alternate ? 0 : static_cast<int>(m_history.size());
}

const TerminalCell& TerminalScreen::cell(int column, int row, int scrolled_back) const
{
    static const TerminalCell nothing;
    if(column < 0 || column >= m_columns || row < 0 || row >= m_rows)
    {
        return nothing;
    }
    // The lines kept and the screen after them, one long run: the view starts `scrolled_back` lines before the
    // screen does.
    const auto at{history_size() - std::clamp(scrolled_back, 0, history_size()) + row};
    if(at < history_size())
    {
        const auto& old{m_history[static_cast<std::size_t>(at)]};
        return static_cast<std::size_t>(column) < old.size() ? old[static_cast<std::size_t>(column)] : nothing;
    }
    return m_lines[static_cast<std::size_t>(at - history_size())][static_cast<std::size_t>(column)];
}

int TerminalScreen::cursor_column() const
{
    return m_cursor_column;
}

int TerminalScreen::cursor_row() const
{
    return m_cursor_row;
}

bool TerminalScreen::cursor_visible() const
{
    return m_cursor_visible;
}

bool TerminalScreen::application_cursor_keys() const
{
    return m_application_cursor_keys;
}

bool TerminalScreen::bracketed_paste() const
{
    return m_bracketed_paste;
}

const std::string& TerminalScreen::title() const
{
    return m_title;
}

std::string TerminalScreen::take_reply()
{
    return std::exchange(m_reply, {});
}

std::uint64_t TerminalScreen::revision() const
{
    return m_revision;
}

TerminalCell TerminalScreen::blank() const
{
    // Erasing leaves the background the program chose, as xterm does.
    TerminalCell cell;
    cell.background = m_pen.inverse ? m_pen.foreground : m_pen.background;
    return cell;
}

TerminalScreen::Line TerminalScreen::blank_line() const
{
    return Line(static_cast<std::size_t>(m_columns), blank());
}

void TerminalScreen::feed(std::string_view bytes)
{
    for(const auto raw : bytes)
    {
        const auto byte{static_cast<unsigned char>(raw)};
        if(m_utf8_left > 0)
        {
            if((byte & 0xC0u) == 0x80u)
            {
                m_utf8_value = (m_utf8_value << 6u) | (byte & 0x3Fu);
                if(-- m_utf8_left == 0)
                {
                    take(m_utf8_value);
                }
                continue;
            }
            // A character cut short: what it had is lost, and this byte starts afresh.
            m_utf8_left = 0;
            take(U'�');
        }
        if(byte < 0x80u)
        {
            take(byte);
        }
        else if((byte & 0xE0u) == 0xC0u)
        {
            m_utf8_left = 1;
            m_utf8_value = byte & 0x1Fu;
        }
        else if((byte & 0xF0u) == 0xE0u)
        {
            m_utf8_left = 2;
            m_utf8_value = byte & 0x0Fu;
        }
        else if((byte & 0xF8u) == 0xF0u)
        {
            m_utf8_left = 3;
            m_utf8_value = byte & 0x07u;
        }
        else
        {
            take(U'�');
        }
    }
    ++ m_revision;
}

void TerminalScreen::take(char32_t character)
{
    switch(m_state)
    {
    case State::ground:
        if(character == 0x1B)
        {
            m_state = State::escape;
            m_intermediate = 0;
        }
        else if(character < 0x20 || character == 0x7F)
        {
            control(character);
        }
        else
        {
            print(character);
        }
        return;
    case State::escape:
        if(character == U'[')
        {
            m_state = State::csi;
            m_parameters.clear();
            m_intermediate = 0;
            m_private = false;
        }
        else if(character == U']')
        {
            m_state = State::osc;
            m_string.clear();
        }
        else if(character == U'P' || character == U'X' || character == U'^' || character == U'_')
        {
            // A device control string, or another the terminal does nothing with, up to its end.
            m_state = State::string;
        }
        else if(character >= 0x20 && character <= 0x2F)
        {
            m_intermediate = static_cast<char>(character);
            m_state = State::escape_intermediate;
        }
        else
        {
            m_state = State::ground;
            escape(character, 0);
        }
        return;
    case State::escape_intermediate:
        if(character >= 0x20 && character <= 0x2F)
        {
            return;
        }
        m_state = State::ground;
        escape(character, m_intermediate);
        return;
    case State::csi:
        if(character == U'?' || character == U'>' || character == U'=' || character == U'<')
        {
            m_private = true;
        }
        else if((character >= U'0' && character <= U'9') || character == U';' || character == U':')
        {
            m_parameters.push_back(static_cast<char>(character));
        }
        else if(character >= 0x20 && character <= 0x2F)
        {
            m_intermediate = static_cast<char>(character);
        }
        else if(character >= 0x40 && character <= 0x7E)
        {
            m_state = State::ground;
            csi(character);
        }
        else if(character < 0x20)
        {
            // A control character inside a sequence acts at once, as a terminal does.
            control(character);
        }
        return;
    case State::osc:
        if(character == 0x07)
        {
            m_state = State::ground;
        }
        else if(character == 0x1B)
        {
            m_state = State::osc_escape;
            return;
        }
        else
        {
            m_string += encode_utf8(character);
            return;
        }
        break;
    case State::osc_escape:
        m_state = State::ground;
        break;
    case State::string:
        if(character == 0x1B)
        {
            m_state = State::string_escape;
        }
        else if(character == 0x07)
        {
            m_state = State::ground;
        }
        return;
    case State::string_escape:
        m_state = character == U'\\' ? State::ground : State::string;
        return;
    }
    // An operating system command ended: 0 and 2 name the window.
    const auto separator{m_string.find(';')};
    if(separator != std::string::npos)
    {
        const auto command{m_string.substr(0, separator)};
        if(command == "0" || command == "2")
        {
            m_title = m_string.substr(separator + 1);
        }
    }
    m_string.clear();
}

void TerminalScreen::control(char32_t character)
{
    switch(character)
    {
    case 0x08:
        m_wrap_pending = false;
        m_cursor_column = std::max(0, m_cursor_column - 1);
        break;
    case 0x09:
    {
        m_wrap_pending = false;
        const auto next{(m_cursor_column / 8 + 1) * 8};
        m_cursor_column = std::min(m_columns - 1, next);
        break;
    }
    case 0x0A:
    case 0x0B:
    case 0x0C:
        line_feed();
        break;
    case 0x0D:
        m_wrap_pending = false;
        m_cursor_column = 0;
        break;
    default:
        break;
    }
}

void TerminalScreen::escape(char32_t final, char intermediate)
{
    // Character sets, as ESC ( B: the terminal writes Unicode whatever it is told.
    if(intermediate != 0)
    {
        return;
    }
    switch(final)
    {
    case U'7':
        save_cursor();
        break;
    case U'8':
        restore_cursor();
        break;
    case U'D':
        line_feed();
        break;
    case U'E':
        line_feed();
        m_cursor_column = 0;
        break;
    case U'M':
        reverse_line_feed();
        break;
    case U'c':
        reset();
        break;
    default:
        break;
    }
}

int TerminalScreen::parameter(std::size_t index, int fallback) const
{
    std::size_t start{};
    for(std::size_t at{}; at < index; ++ at)
    {
        const auto next{m_parameters.find(';', start)};
        if(next == std::string::npos)
        {
            return fallback;
        }
        start = next + 1;
    }
    const auto end{std::min(m_parameters.find(';', start), m_parameters.size())};
    int value{};
    const auto [ptr, error]{std::from_chars(m_parameters.data() + start, m_parameters.data() + end, value)};
    static_cast<void>(ptr);
    return error != std::errc{} || value == 0 ? fallback : value;
}

void TerminalScreen::csi(char32_t final)
{
    if(m_private)
    {
        if(final == U'h' || final == U'l')
        {
            std::size_t start{};
            while(start <= m_parameters.size())
            {
                const auto end{std::min(m_parameters.find(';', start), m_parameters.size())};
                int mode{};
                std::from_chars(m_parameters.data() + start, m_parameters.data() + end, mode);
                private_mode(mode, final == U'h');
                start = end + 1;
            }
        }
        else if(final == U'c' && !m_parameters.empty() && m_parameters.front() != '0')
        {
            return;
        }
        else if(final == U'c')
        {
            // The secondary device attributes: a VT220, of no particular version.
            m_reply += "\x1b[>1;10;0c";
        }
        return;
    }
    if(m_intermediate != 0)
    {
        // Cursor shapes and the like (CSI Ps SP q), which a cell cursor ignores.
        return;
    }
    const auto count{parameter(0, 1)};
    switch(final)
    {
    case U'@':
        insert_characters(count);
        break;
    case U'A':
        move_to(m_cursor_column, std::max(m_cursor_row >= m_top ? m_top : 0, m_cursor_row - count));
        break;
    case U'B':
    case U'e':
        move_to(m_cursor_column, std::min(m_cursor_row <= m_bottom ? m_bottom : m_rows - 1, m_cursor_row + count));
        break;
    case U'C':
    case U'a':
        move_to(m_cursor_column + count, m_cursor_row);
        break;
    case U'D':
        move_to(m_cursor_column - count, m_cursor_row);
        break;
    case U'E':
        move_to(0, m_cursor_row + count);
        break;
    case U'F':
        move_to(0, m_cursor_row - count);
        break;
    case U'G':
    case U'`':
        move_to(count - 1, m_cursor_row);
        break;
    case U'H':
    case U'f':
        move_to(parameter(1, 1) - 1, count - 1);
        break;
    case U'J':
        erase_display(parameter(0, 0));
        break;
    case U'K':
        erase_line(parameter(0, 0));
        break;
    case U'L':
        insert_lines(count);
        break;
    case U'M':
        delete_lines(count);
        break;
    case U'P':
        delete_characters(count);
        break;
    case U'S':
        scroll_up(count);
        break;
    case U'T':
        scroll_down(count);
        break;
    case U'X':
        erase_characters(count);
        break;
    case U'd':
        move_to(m_cursor_column, count - 1);
        break;
    case U'm':
        sgr();
        break;
    case U'n':
        if(parameter(0, 0) == 5)
        {
            m_reply += "\x1b[0n";
        }
        else if(parameter(0, 0) == 6)
        {
            m_reply += "\x1b[" + std::to_string(m_cursor_row + 1) + ";" + std::to_string(m_cursor_column + 1) + "R";
        }
        break;
    case U'c':
        // The primary device attributes: a VT100 with the advanced video option, as most terminals say.
        m_reply += "\x1b[?1;2c";
        break;
    case U'r':
    {
        const auto top{parameter(0, 1) - 1};
        const auto bottom{parameter(1, m_rows) - 1};
        if(top < bottom && bottom < m_rows)
        {
            m_top = top;
            m_bottom = bottom;
            move_to(0, 0);
        }
        break;
    }
    case U's':
        save_cursor();
        break;
    case U'u':
        restore_cursor();
        break;
    default:
        break;
    }
}

void TerminalScreen::private_mode(int mode, bool set)
{
    switch(mode)
    {
    case 1:
        m_application_cursor_keys = set;
        break;
    case 7:
        m_autowrap = set;
        break;
    case 25:
        m_cursor_visible = set;
        break;
    case 47:
    case 1047:
        use_alternate(set);
        break;
    case 1049:
        // The whole screen for the program, as an editor takes it, and the cursor where it was once it gives it back.
        if(set)
        {
            save_cursor();
            use_alternate(true);
            erase_display(2);
        }
        else
        {
            use_alternate(false);
            restore_cursor();
        }
        break;
    case 2004:
        m_bracketed_paste = set;
        break;
    default:
        break;
    }
}

void TerminalScreen::sgr()
{
    if(m_parameters.empty())
    {
        m_pen = TerminalCell{};
        return;
    }
    std::vector<int> values;
    std::size_t start{};
    while(start <= m_parameters.size())
    {
        auto end{m_parameters.find_first_of(";:", start)};
        end = end == std::string::npos ? m_parameters.size() : end;
        int value{};
        std::from_chars(m_parameters.data() + start, m_parameters.data() + end, value);
        values.push_back(value);
        start = end + 1;
    }
    for(std::size_t index{}; index < values.size(); ++ index)
    {
        const auto value{values[index]};
        // 38 and 48 name a colour in the next numbers: 5 and an index, or 2 and its red, green and blue.
        const auto extended{[&values, &index]() -> std::optional<TerminalColor>
                            {
                                if(index + 2 < values.size() && values[index + 1] == 5)
                                {
                                    index += 2;
                                    return named(values[index]);
                                }
                                if(index + 4 < values.size() && values[index + 1] == 2)
                                {
                                    const auto channel{[](int number)
                                                       { return static_cast<std::uint8_t>(std::clamp(number, 0, 255)); }};
                                    TerminalColor color{false,
                                                        {channel(values[index + 2]), channel(values[index + 3]),
                                                         channel(values[index + 4])}};
                                    index += 4;
                                    return color;
                                }
                                return std::nullopt;
                            }};
        if(value == 0)
        {
            m_pen = TerminalCell{};
        }
        else if(value == 1)
        {
            m_pen.bold = true;
        }
        else if(value == 2)
        {
            m_pen.faint = true;
        }
        else if(value == 3)
        {
            m_pen.italic = true;
        }
        else if(value == 4)
        {
            m_pen.underline = true;
        }
        else if(value == 7)
        {
            m_pen.inverse = true;
        }
        else if(value == 22)
        {
            m_pen.bold = m_pen.faint = false;
        }
        else if(value == 23)
        {
            m_pen.italic = false;
        }
        else if(value == 24)
        {
            m_pen.underline = false;
        }
        else if(value == 27)
        {
            m_pen.inverse = false;
        }
        else if(value >= 30 && value <= 37)
        {
            m_pen.foreground = named(value - 30);
        }
        else if(value == 38)
        {
            if(const auto color{extended()})
            {
                m_pen.foreground = *color;
            }
        }
        else if(value == 39)
        {
            m_pen.foreground = {};
        }
        else if(value >= 40 && value <= 47)
        {
            m_pen.background = named(value - 40);
        }
        else if(value == 48)
        {
            if(const auto color{extended()})
            {
                m_pen.background = *color;
            }
        }
        else if(value == 49)
        {
            m_pen.background = {};
        }
        else if(value >= 90 && value <= 97)
        {
            m_pen.foreground = named(value - 90 + 8);
        }
        else if(value >= 100 && value <= 107)
        {
            m_pen.background = named(value - 100 + 8);
        }
    }
}

void TerminalScreen::print(char32_t character)
{
    const auto width{terminal_cell_width(character)};
    if(width <= 0)
    {
        // A combining accent joins the letter before it, when there is a character for both.
        const auto column{m_wrap_pending ? m_cursor_column : m_cursor_column - 1};
        if(column >= 0)
        {
            auto& before{m_lines[static_cast<std::size_t>(m_cursor_row)][static_cast<std::size_t>(column)]};
            if(const auto composed{compose(before.glyph, character)})
            {
                before.glyph = *composed;
            }
        }
        return;
    }
    if(m_wrap_pending || (width == 2 && m_cursor_column == m_columns - 1))
    {
        if(m_autowrap)
        {
            line_feed();
            m_cursor_column = 0;
        }
        m_wrap_pending = false;
    }
    auto& line{m_lines[static_cast<std::size_t>(m_cursor_row)]};
    auto cell{m_pen};
    if(cell.bold && cell.foreground.base >= 0)
    {
        cell.foreground = {false, palette(cell.foreground.base + 8)};
    }
    cell.glyph = character;
    cell.continuation = false;
    line[static_cast<std::size_t>(m_cursor_column)] = cell;
    if(width == 2 && m_cursor_column + 1 < m_columns)
    {
        auto right{cell};
        right.glyph = U' ';
        right.continuation = true;
        line[static_cast<std::size_t>(m_cursor_column + 1)] = right;
    }
    if(m_cursor_column + width >= m_columns)
    {
        m_cursor_column = m_columns - 1;
        m_wrap_pending = true;
    }
    else
    {
        m_cursor_column += width;
    }
}

void TerminalScreen::line_feed()
{
    m_wrap_pending = false;
    if(m_cursor_row == m_bottom)
    {
        scroll_up(1);
    }
    else if(m_cursor_row < m_rows - 1)
    {
        ++ m_cursor_row;
    }
}

void TerminalScreen::reverse_line_feed()
{
    m_wrap_pending = false;
    if(m_cursor_row == m_top)
    {
        scroll_down(1);
    }
    else if(m_cursor_row > 0)
    {
        -- m_cursor_row;
    }
}

void TerminalScreen::scroll_up(int count)
{
    count = std::clamp(count, 0, m_bottom - m_top + 1);
    for(int step{}; step < count; ++ step)
    {
        auto gone{std::move(m_lines[static_cast<std::size_t>(m_top)])};
        // What leaves the top of the whole screen is kept, to look back at.
        if(!m_alternate && m_top == 0)
        {
            m_history.push_back(std::move(gone));
            if(m_history.size() > history_limit)
            {
                m_history.pop_front();
            }
        }
        m_lines.erase(m_lines.begin() + m_top);
        m_lines.insert(m_lines.begin() + m_bottom, blank_line());
    }
}

void TerminalScreen::scroll_down(int count)
{
    count = std::clamp(count, 0, m_bottom - m_top + 1);
    for(int step{}; step < count; ++ step)
    {
        m_lines.erase(m_lines.begin() + m_bottom);
        m_lines.insert(m_lines.begin() + m_top, blank_line());
    }
}

void TerminalScreen::erase_display(int how)
{
    if(how == 3)
    {
        m_history.clear();
        return;
    }
    const auto from{how == 0 ? m_cursor_row + 1 : 0};
    const auto to{how == 1 ? m_cursor_row : m_rows};
    if(how != 2)
    {
        erase_line(how);
    }
    for(auto row{from}; row < to; ++ row)
    {
        if(how == 2 || row != m_cursor_row)
        {
            m_lines[static_cast<std::size_t>(row)] = blank_line();
        }
    }
}

void TerminalScreen::erase_line(int how)
{
    auto& line{m_lines[static_cast<std::size_t>(m_cursor_row)]};
    const auto from{how == 0 ? m_cursor_column : 0};
    const auto to{how == 1 ? m_cursor_column + 1 : m_columns};
    std::fill(line.begin() + from, line.begin() + std::min(to, m_columns), blank());
    m_wrap_pending = false;
}

void TerminalScreen::insert_lines(int count)
{
    if(m_cursor_row < m_top || m_cursor_row > m_bottom)
    {
        return;
    }
    count = std::clamp(count, 0, m_bottom - m_cursor_row + 1);
    for(int step{}; step < count; ++ step)
    {
        m_lines.erase(m_lines.begin() + m_bottom);
        m_lines.insert(m_lines.begin() + m_cursor_row, blank_line());
    }
    m_cursor_column = 0;
}

void TerminalScreen::delete_lines(int count)
{
    if(m_cursor_row < m_top || m_cursor_row > m_bottom)
    {
        return;
    }
    count = std::clamp(count, 0, m_bottom - m_cursor_row + 1);
    for(int step{}; step < count; ++ step)
    {
        m_lines.erase(m_lines.begin() + m_cursor_row);
        m_lines.insert(m_lines.begin() + m_bottom, blank_line());
    }
    m_cursor_column = 0;
}

void TerminalScreen::insert_characters(int count)
{
    auto& line{m_lines[static_cast<std::size_t>(m_cursor_row)]};
    count = std::clamp(count, 0, m_columns - m_cursor_column);
    line.insert(line.begin() + m_cursor_column, static_cast<std::size_t>(count), blank());
    line.resize(static_cast<std::size_t>(m_columns));
}

void TerminalScreen::delete_characters(int count)
{
    auto& line{m_lines[static_cast<std::size_t>(m_cursor_row)]};
    count = std::clamp(count, 0, m_columns - m_cursor_column);
    line.erase(line.begin() + m_cursor_column, line.begin() + m_cursor_column + count);
    line.resize(static_cast<std::size_t>(m_columns), blank());
}

void TerminalScreen::erase_characters(int count)
{
    auto& line{m_lines[static_cast<std::size_t>(m_cursor_row)]};
    const auto to{std::min(m_columns, m_cursor_column + std::max(0, count))};
    std::fill(line.begin() + m_cursor_column, line.begin() + to, blank());
}

void TerminalScreen::move_to(int column, int row)
{
    m_wrap_pending = false;
    m_cursor_column = std::clamp(column, 0, m_columns - 1);
    m_cursor_row = std::clamp(row, 0, m_rows - 1);
}

void TerminalScreen::use_alternate(bool alternate)
{
    if(alternate == m_alternate)
    {
        return;
    }
    std::swap(m_lines, m_other_lines);
    std::swap(m_saved, m_other_saved);
    m_alternate = alternate;
    if(alternate)
    {
        for(auto& line : m_lines)
        {
            line = blank_line();
        }
    }
}

void TerminalScreen::reset()
{
    use_alternate(false);
    m_pen = TerminalCell{};
    m_lines.assign(static_cast<std::size_t>(m_rows), blank_line());
    m_history.clear();
    m_top = 0;
    m_bottom = m_rows - 1;
    m_cursor_column = m_cursor_row = 0;
    m_wrap_pending = false;
    m_cursor_visible = m_autowrap = true;
    m_application_cursor_keys = m_bracketed_paste = false;
}

void TerminalScreen::save_cursor()
{
    m_saved = {m_cursor_column, m_cursor_row, m_pen};
}

void TerminalScreen::restore_cursor()
{
    m_pen = m_saved.pen;
    move_to(m_saved.column, m_saved.row);
}

void TerminalScreen::resize(int columns, int rows)
{
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    if(columns == m_columns && rows == m_rows)
    {
        return;
    }
    const auto fit{[this, columns, rows](std::vector<Line>& lines, bool keep_history)
                   {
                       for(auto& line : lines)
                       {
                           line.resize(static_cast<std::size_t>(columns), blank());
                       }
                       // A screen growing shorter loses lines from the top, into the history, keeping the cursor's.
                       while(static_cast<int>(lines.size()) > rows)
                       {
                           if(keep_history && m_cursor_row > 0)
                           {
                               m_history.push_back(std::move(lines.front()));
                               lines.erase(lines.begin());
                               -- m_cursor_row;
                           }
                           else
                           {
                               lines.pop_back();
                           }
                       }
                       while(static_cast<int>(lines.size()) < rows)
                       {
                           lines.push_back(Line(static_cast<std::size_t>(columns), TerminalCell{}));
                       }
                   }};
    fit(m_lines, !m_alternate);
    fit(m_other_lines, false);
    m_columns = columns;
    m_rows = rows;
    m_top = 0;
    m_bottom = rows - 1;
    m_cursor_column = std::min(m_cursor_column, columns - 1);
    m_cursor_row = std::min(m_cursor_row, rows - 1);
    m_wrap_pending = false;
    ++ m_revision;
}

} // namespace olagarro::oui
