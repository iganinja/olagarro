#include <oui/terminal_view.h>

#include <oui/application.h>
#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>
#include <oui/utf8.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

namespace
{

// The number xterm adds to a key's sequence for its modifiers: 2 Shift, 3 Alt, 5 Ctrl, and their sums.
[[nodiscard]] int modifier_code(bool shift, bool alt, bool control)
{
    return 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (control ? 4 : 0);
}

// A cursor key, Home or End: ESC [ X, ESC O X in the application's form, ESC [ 1 ; m X with modifiers.
[[nodiscard]] std::string cursor_key(char letter, const KeyEvent& key, bool application)
{
    const auto modifiers{modifier_code(key.shift, key.alt, key.control)};
    if(modifiers > 1)
    {
        return std::string{"\x1b[1;"} + std::to_string(modifiers) + letter;
    }
    return std::string{application ? "\x1bO" : "\x1b["} + letter;
}

// A key sent as ESC [ n ~, with modifiers ESC [ n ; m ~.
[[nodiscard]] std::string tilde_key(int number, const KeyEvent& key)
{
    const auto modifiers{modifier_code(key.shift, key.alt, key.control)};
    return "\x1b[" + std::to_string(number) + (modifiers > 1 ? ";" + std::to_string(modifiers) : std::string{}) + "~";
}

// F1 to F4: ESC O P to S, with modifiers ESC [ 1 ; m P to S.
[[nodiscard]] std::string function_key(char letter, const KeyEvent& key)
{
    const auto modifiers{modifier_code(key.shift, key.alt, key.control)};
    if(modifiers > 1)
    {
        return std::string{"\x1b[1;"} + std::to_string(modifiers) + letter;
    }
    return std::string{"\x1bO"} + letter;
}

[[nodiscard]] Color resolve(const TerminalColor& color, Color fallback)
{
    return color.is_default ? fallback : color.rgb;
}

} // namespace

std::string terminal_keys(const Event& event, bool application_cursor_keys)
{
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        auto character{text->codepoint};
        std::string bytes;
        if(text->control)
        {
            // Ctrl and a letter is that letter's control character; the signs about it, the rest of them.
            if(character >= U'a' && character <= U'z')
            {
                character = character - U'a' + 1;
            }
            else if(character >= U'A' && character <= U'Z')
            {
                character = character - U'A' + 1;
            }
            else if(character == U'@' || character == U'2')
            {
                character = 0;
            }
            else if(character == U'[' || character == U'3')
            {
                character = 0x1B;
            }
            else if(character == U'\\' || character == U'4')
            {
                character = 0x1C;
            }
            else if(character == U']' || character == U'5')
            {
                character = 0x1D;
            }
            else if(character == U'^' || character == U'6')
            {
                character = 0x1E;
            }
            else if(character == U'_' || character == U'7' || character == U'/')
            {
                character = 0x1F;
            }
            else if(character == U'8' || character == U'?')
            {
                character = 0x7F;
            }
        }
        if(text->alt)
        {
            bytes += '\x1b';
        }
        if(character == 0)
        {
            bytes += '\0';
        }
        else
        {
            bytes += encode_utf8(character);
        }
        return bytes;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr)
    {
        return {};
    }
    const std::string alt{key->alt ? "\x1b" : ""};
    switch(key->key)
    {
    case Key::enter:
        return alt + "\r";
    case Key::tab:
        return alt + "\t";
    case Key::shift_tab:
        return "\x1b[Z";
    case Key::backspace:
        return alt + (key->control ? "\x08" : "\x7f");
    case Key::escape:
        return alt + "\x1b";
    case Key::space:
        return alt + (key->control ? std::string(1, '\0') : std::string{" "});
    case Key::up:
        return cursor_key('A', *key, application_cursor_keys);
    case Key::down:
        return cursor_key('B', *key, application_cursor_keys);
    case Key::right:
        return cursor_key('C', *key, application_cursor_keys);
    case Key::left:
        return cursor_key('D', *key, application_cursor_keys);
    case Key::home:
        return cursor_key('H', *key, application_cursor_keys);
    case Key::end:
        return cursor_key('F', *key, application_cursor_keys);
    case Key::insert_:
        return tilde_key(2, *key);
    case Key::delete_:
        return tilde_key(3, *key);
    case Key::page_up:
        return tilde_key(5, *key);
    case Key::page_down:
        return tilde_key(6, *key);
    case Key::f1:
        return function_key('P', *key);
    case Key::f2:
        return function_key('Q', *key);
    case Key::f3:
        return function_key('R', *key);
    case Key::f4:
        return function_key('S', *key);
    case Key::f5:
        return tilde_key(15, *key);
    case Key::f6:
        return tilde_key(17, *key);
    case Key::f7:
        return tilde_key(18, *key);
    case Key::f8:
        return tilde_key(19, *key);
    case Key::f9:
        return tilde_key(20, *key);
    case Key::f10:
        return tilde_key(21, *key);
    case Key::f11:
        return tilde_key(23, *key);
    case Key::f12:
        return tilde_key(24, *key);
    }
    return {};
}

TerminalView::TerminalView(Application& application, PtyOptions options) : m_application{application}
{
    if(options.command.empty())
    {
        options.command = user_shell();
    }
    options.columns = m_screen.columns();
    options.rows = m_screen.rows();
    m_pty = start_pty(application, std::move(options),
                      {[this](std::string_view text) { take_output(text); },
                       [this](int exit_code)
                       {
                           const auto ended{exit_code == 0 ? translate("TerminalView", "ended")
                                                           : format_text(translate("TerminalView", "ended with {0}"),
                                                                         {std::to_string(exit_code)})};
                           m_screen.feed("\r\n[" + ended + "]");
                           m_finished.fire(exit_code);
                       }});
    m_title_timer = application.start_timer(std::chrono::milliseconds{500}, [this] { refresh_title(); });
}

TerminalView::~TerminalView() = default;

Size TerminalView::measure(const Constraints& constraints) const
{
    return constraints.constrain({std::max(1, constraints.max_width), std::max(1, constraints.max_height)});
}

void TerminalView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto columns{std::max(1, bounds.width)};
    const auto rows{std::max(1, bounds.height)};
    if(columns != m_screen.columns() || rows != m_screen.rows())
    {
        m_screen.resize(columns, rows);
        m_scrolled_back = std::min(m_scrolled_back, m_screen.history_size());
        if(m_pty)
        {
            m_pty->resize(columns, rows);
        }
    }
}

void TerminalView::paint(Painter& painter, const Theme& theme) const
{
    const auto& area{bounds()};
    painter.fill_rect(area, theme.editor_background);
    const auto rows{std::min(area.height, m_screen.rows())};
    const auto columns{std::min(area.width, m_screen.columns())};
    const auto show_cursor{is_focused() && m_screen.cursor_visible() && m_scrolled_back == 0};
    for(int row{}; row < rows; ++ row)
    {
        // A run of cells drawn alike goes out as one piece of text.
        std::string run;
        TextStyle run_style;
        int run_start{};
        const auto flush{[&painter, &area, &run, &run_style, &run_start, row]
                         {
                             if(!run.empty())
                             {
                                 painter.draw_text({area.x + run_start, area.y + row}, run, run_style);
                                 run.clear();
                             }
                         }};
        for(int column{}; column < columns; ++ column)
        {
            const auto& cell{m_screen.cell(column, row, m_scrolled_back)};
            if(cell.continuation)
            {
                continue;
            }
            auto foreground{resolve(cell.foreground, theme.editor_foreground)};
            auto background{resolve(cell.background, theme.editor_background)};
            const auto at_cursor{show_cursor && row == m_screen.cursor_row() && column == m_screen.cursor_column()};
            if(cell.inverse != at_cursor)
            {
                std::swap(foreground, background);
            }
            if(is_selected(m_screen.history_size() - m_scrolled_back + row, column))
            {
                foreground = theme.selection_foreground;
                background = theme.selection_background;
            }
            TextStyle style{foreground, background,
                            static_cast<std::uint8_t>((cell.bold ? text_bold : 0) |
                                                      (cell.underline ? text_underline : 0) |
                                                      (cell.italic ? text_italic : 0) | (cell.faint ? text_dim : 0))};
            if(run.empty() || style.foreground != run_style.foreground || style.background != run_style.background ||
               style.attributes != run_style.attributes)
            {
                flush();
                run_style = style;
                run_start = column;
            }
            run += encode_utf8(cell.glyph);
        }
        flush();
    }
}

bool TerminalView::is_focusable() const
{
    return true;
}

bool TerminalView::claims_key(const Event& event) const
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        // Moving between the application's tabs stays the application's.
        if(key->control && (key->key == Key::page_up || key->key == Key::page_down || key->key == Key::tab ||
                            key->key == Key::shift_tab))
        {
            return false;
        }
        return !(key->control && key->shift);
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        // Ctrl and Shift with a letter is the application's, but for the terminal's own paste.
        return !(text->control && text->shift) || text->codepoint == U'v' || text->codepoint == U'V' ||
               text->codepoint == U'c' || text->codepoint == U'C';
    }
    return false;
}

bool TerminalView::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(mouse->wheel_delta == 0)
        {
            return handle_selection(*mouse);
        }
        // A program with the whole screen to itself, as an editor or a pager, scrolls with its own arrow keys.
        if(m_screen.history_size() == 0)
        {
            send(terminal_keys(KeyEvent{mouse->wheel_delta > 0 ? Key::up : Key::down},
                               m_screen.application_cursor_keys()));
        }
        else
        {
            scroll_back(mouse->wheel_delta);
        }
        return true;
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(key->shift && !key->control && !key->alt && (key->key == Key::page_up || key->key == Key::page_down))
        {
            scroll_back((key->key == Key::page_up ? 1 : -1) * std::max(1, m_screen.rows() - 1));
            return true;
        }
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)}; text != nullptr && text->control && text->shift)
    {
        if(text->codepoint == U'v' || text->codepoint == U'V')
        {
            paste(m_application.clipboard().text());
            return true;
        }
        if(text->codepoint == U'c' || text->codepoint == U'C')
        {
            copy_selection();
            return true;
        }
        return false;
    }
    const auto bytes{terminal_keys(event, m_screen.application_cursor_keys())};
    if(bytes.empty())
    {
        return false;
    }
    // Typing goes back to the screen as the program has it, and leaves what was selected.
    m_scrolled_back = 0;
    m_anchor.reset();
    send(bytes);
    return true;
}

bool TerminalView::handle_selection(const MouseEvent& mouse)
{
    if(mouse.pressed && mouse.right)
    {
        paste(m_application.clipboard().text());
        return true;
    }
    if(mouse.pressed)
    {
        const auto place{place_at(mouse.x, mouse.y)};
        m_selecting = true;
        m_dragged = mouse.clicks > 1;
        m_anchor = place;
        m_head = place;
        if(mouse.clicks == 2)
        {
            select_word(place);
        }
        else if(mouse.clicks >= 3)
        {
            m_anchor = Place{place.line, 0};
            m_head = Place{place.line, m_screen.columns() - 1};
        }
        return true;
    }
    if(mouse.dragging && m_selecting)
    {
        // Dragged past the top or the bottom, the view scrolls to follow.
        if(mouse.y < bounds().y)
        {
            scroll_back(1);
        }
        else if(mouse.y >= bounds().y + bounds().height)
        {
            scroll_back(-1);
        }
        m_head = place_at(mouse.x, mouse.y);
        m_dragged = m_dragged || !(m_head == *m_anchor);
        return true;
    }
    if(mouse.released && m_selecting)
    {
        m_selecting = false;
        // A click with no drag selects nothing; what was selected goes to the clipboard, as terminals do.
        if(m_dragged)
        {
            copy_selection();
        }
        else
        {
            m_anchor.reset();
        }
        return true;
    }
    return mouse.pressed;
}

TerminalView::Place TerminalView::place_at(int x, int y) const
{
    const auto column{std::clamp(x - bounds().x, 0, std::max(0, m_screen.columns() - 1))};
    const auto row{std::clamp(y - bounds().y, 0, std::max(0, m_screen.rows() - 1))};
    return {m_screen.history_size() - m_scrolled_back + row, column};
}

const TerminalCell& TerminalView::cell_at(const Place& place) const
{
    const auto history{m_screen.history_size()};
    if(place.line < history)
    {
        return m_screen.cell(place.column, 0, history - place.line);
    }
    return m_screen.cell(place.column, place.line - history);
}

bool TerminalView::is_selected(int line, int column) const
{
    if(!m_anchor)
    {
        return false;
    }
    const auto first{std::min(*m_anchor, m_head)};
    const auto last{std::max(*m_anchor, m_head)};
    const Place place{line, column};
    return !(place < first) && !(last < place);
}

std::string TerminalView::selected_text() const
{
    if(!m_anchor)
    {
        return {};
    }
    const auto first{std::min(*m_anchor, m_head)};
    const auto last{std::max(*m_anchor, m_head)};
    std::string text;
    for(auto line{first.line}; line <= last.line; ++ line)
    {
        std::string row;
        const auto from{line == first.line ? first.column : 0};
        const auto to{line == last.line ? last.column : m_screen.columns() - 1};
        for(auto column{from}; column <= to; ++ column)
        {
            const auto& cell{cell_at({line, column})};
            if(!cell.continuation)
            {
                row += encode_utf8(cell.glyph);
            }
        }
        row.erase(row.find_last_not_of(' ') + 1);
        if(line > first.line)
        {
            text += '\n';
        }
        text += row;
    }
    return text;
}

void TerminalView::select_word(const Place& place)
{
    // A word runs to the spaces and the marks that set words apart in a command line.
    constexpr std::u32string_view apart{U" \t()[]{}<>'\"`|;,="};
    const auto in_word{[this, &place, &apart](int column)
                       { return apart.find(cell_at({place.line, column}).glyph) == std::u32string_view::npos; }};
    if(!in_word(place.column))
    {
        return;
    }
    auto first{place.column};
    auto last{place.column};
    while(first > 0 && in_word(first - 1))
    {
        -- first;
    }
    while(last + 1 < m_screen.columns() && in_word(last + 1))
    {
        ++ last;
    }
    m_anchor = Place{place.line, first};
    m_head = Place{place.line, last};
}

void TerminalView::copy_selection()
{
    if(const auto text{selected_text()}; !text.empty())
    {
        static_cast<void>(m_application.clipboard().set_text(text));
    }
}

void TerminalView::paste(std::string_view text)
{
    std::string bytes;
    // A pasted line ends as Enter does, with a carriage return.
    for(const auto character : text)
    {
        if(character != '\r')
        {
            bytes += character == '\n' ? '\r' : character;
        }
    }
    if(m_screen.bracketed_paste())
    {
        bytes = "\x1b[200~" + bytes + "\x1b[201~";
    }
    m_scrolled_back = 0;
    m_anchor.reset();
    send(bytes);
}

bool TerminalView::is_running() const
{
    return m_pty && m_pty->is_running();
}

const std::string& TerminalView::title() const
{
    return m_title;
}

const TerminalScreen& TerminalView::screen() const
{
    return m_screen;
}

void TerminalView::take_output(std::string_view text)
{
    const auto history_before{m_screen.history_size()};
    m_screen.feed(text);
    // Looking back, the view stays on the same lines while new ones come.
    if(m_scrolled_back > 0)
    {
        m_scrolled_back = std::min(m_screen.history_size(), m_scrolled_back + m_screen.history_size() - history_before);
    }
    if(const auto reply{m_screen.take_reply()}; !reply.empty())
    {
        send(reply);
    }
    // Output follows starting a program and ending one, so the title is looked at again with it.
    refresh_title();
}

void TerminalView::refresh_title()
{
    auto title{m_screen.title()};
    if(title.empty() && m_pty)
    {
        title = m_pty->foreground_program();
    }
    if(title != m_title)
    {
        m_title = std::move(title);
        m_title_changed.fire(m_title);
    }
}

void TerminalView::send(std::string_view bytes)
{
    if(m_pty && !bytes.empty())
    {
        m_pty->write(bytes);
    }
}

void TerminalView::scroll_back(int lines)
{
    m_scrolled_back = std::clamp(m_scrolled_back + lines, 0, m_screen.history_size());
}

} // namespace olagarro::oui
