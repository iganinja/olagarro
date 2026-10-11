#include <oui/line_edit.h>
#include <oui/text_metrics.h>

#include <oui/utf8.h>
#include <algorithm>

#include "unicode_composition.h"
#include "word_boundary.h"

namespace olagarro::oui
{
void LineEdit::set_text(std::string_view text)
{
    m_text = text;
    m_cursor = m_text.size();
    m_anchor = m_cursor;
    m_changed.fire(m_text);
}
std::string_view LineEdit::text() const
{
    return m_text;
}
std::string LineEdit::selected_text() const
{
    const auto [first, last]{selection_range()};
    return m_text.substr(first, last - first);
}
void LineEdit::select_all()
{
    m_anchor = 0;
    m_cursor = m_text.size();
}
void LineEdit::select_range(std::size_t start, std::size_t end)
{
    m_anchor = std::min(start, m_text.size());
    m_cursor = std::min(end, m_text.size());
}
void LineEdit::copy_selection(std::string& clipboard) const
{
    clipboard = selected_text();
}
void LineEdit::cut_selection(std::string& clipboard)
{
    copy_selection(clipboard);
    static_cast<void>(erase_selection());
}
void LineEdit::paste(std::string_view text)
{
    replace_selection(text);
}
void LineEdit::set_placeholder(std::string_view placeholder)
{
    m_placeholder = placeholder;
}
void LineEdit::set_background(ColorRef background)
{
    m_background = background;
}
bool LineEdit::is_focusable() const
{
    return true;
}
bool LineEdit::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(mouse->released)
        {
            const auto result{m_mouse_selecting};
            m_mouse_selecting = false;
            return result;
        }
        if((!mouse->pressed && (!mouse->dragging || !m_mouse_selecting)) || mouse->x < bounds().x ||
           mouse->x >= bounds().x + bounds().width || mouse->y != bounds().y)
        {
            return false;
        }
        // A drag takes in the character under the pointer.
        const auto& metrics{text_metrics()};
        const auto first{std::min(m_first_shown, m_text.size())};
        m_cursor = first + metrics.byte_at(std::string_view{m_text}.substr(first),
                                           (mouse->x - bounds().x) * metrics.cell_width() + mouse->cell_offset -
                                               metrics.field_padding(),
                                           !mouse->pressed);
        if(mouse->pressed)
        {
            m_anchor = m_cursor;
            m_mouse_selecting = true;
        }
        return true;
    }
    if(const auto* const text_event{std::get_if<TextEvent>(&event)})
    {
        if(text_event->control || text_event->alt)
        {
            return false;
        }
        // A combining accent typed after its letter (NFD input) merges into the precomposed character.
        if(is_diacritic(text_event->codepoint) && m_cursor == m_anchor && m_cursor > 0)
        {
            const auto base_start{previous_codepoint(m_cursor)};
            auto base_end{base_start};
            if(const auto composed{compose(decode_utf8(m_text, base_end), text_event->codepoint)})
            {
                m_anchor = base_start;
                replace_selection(encode_utf8(*composed));
                return true;
            }
        }
        replace_selection(encode_utf8(text_event->codepoint));
        return true;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr)
    {
        return false;
    }
    if(key->key == Key::space)
    {
        replace_selection(" ");
        return true;
    }
    if(key->key == Key::left)
    {
        if(key->control)
        {
            move_word(false, key->shift);
        }
        else
        {
            move_left(key->shift);
        }
        return true;
    }
    if(key->key == Key::right)
    {
        if(key->control)
        {
            move_word(true, key->shift);
        }
        else
        {
            move_right(key->shift);
        }
        return true;
    }
    if(key->key == Key::backspace)
    {
        if(erase_selection())
        {
            return true;
        }
        if(m_cursor == 0)
        {
            return false;
        }
        m_anchor = previous_codepoint(m_cursor);
        return erase_selection();
    }
    if(key->key == Key::delete_)
    {
        if(erase_selection())
        {
            return true;
        }
        if(m_cursor == m_text.size())
        {
            return false;
        }
        m_anchor = m_cursor;
        static_cast<void>(decode_utf8(m_text, m_anchor));
        return erase_selection();
    }
    if(key->key == Key::home)
    {
        m_cursor = 0;
        if(!key->shift)
        {
            m_anchor = 0;
        }
        return true;
    }
    if(key->key == Key::end)
    {
        m_cursor = m_text.size();
        if(!key->shift)
        {
            m_anchor = m_cursor;
        }
        return true;
    }
    if(key->key == Key::enter)
    {
        m_submitted.fire(m_text);
        return true;
    }
    return false;
}
Size LineEdit::measure(const Constraints& constraints) const
{
    // As wide as its text shows, in whole cells, with the padding before it and room for the cursor after: in a
    // proportional font text takes fewer cells than it has characters.
    const auto& metrics{text_metrics()};
    const auto units{std::max(metrics.width(m_text), metrics.width(m_placeholder)) + metrics.field_padding()};
    const auto cell{std::max(1, metrics.cell_width())};
    const auto content_width{(units + cell - 1) / cell};
    return constraints.constrain({std::max(content_width + 1, 4), 1});
}
void LineEdit::keep_cursor_shown() const
{
    const auto& metrics{text_metrics()};
    const std::string_view text{m_text};
    // What fits after the padding, leaving a cell for the cursor at the end.
    const auto room{std::max(1, (bounds().width - 1) * metrics.cell_width() - metrics.field_padding())};
    auto first{std::min({m_first_shown, m_cursor, m_text.size()})};
    while(first < m_cursor && metrics.width(text.substr(first, m_cursor - first)) > room)
    {
        static_cast<void>(decode_utf8(text, first));
    }
    // Text taken away leaves room: what comes before shows again.
    while(first > 0 && metrics.width(text.substr(previous_codepoint(first))) <= room)
    {
        first = previous_codepoint(first);
    }
    m_first_shown = first;
}
void LineEdit::paint(Painter& painter, const Theme& theme) const
{
    // A field without the focus shows its text from the start, as it reads; with it, where the cursor is.
    if(is_focused())
    {
        keep_cursor_shown();
    }
    else
    {
        m_first_shown = 0;
    }
    const auto focused{is_focused()};
    const auto background_color{focused ? theme.focus_background
                                        : resolve(m_background, theme, theme.input_background)};
    const auto foreground_color{!is_enabled() ? theme.disabled_foreground
                                : focused     ? theme.focus_text
                                              : theme.input_foreground};
    const auto selection_fg{theme.item_selected_foreground};
    const auto selection_bg{theme.item_selected_background};
    const auto placeholder{m_text.empty()};
    const auto [first, last]{selection_range()};
    TextFieldLook look;
    // From the first byte shown; the selection and the cursor count from there.
    const auto shown{placeholder ? 0 : m_first_shown};
    look.text = placeholder ? std::string_view{m_placeholder} : std::string_view{m_text}.substr(shown);
    look.selection_start = placeholder ? 0 : std::max(first, shown) - shown;
    look.selection_end = placeholder ? 0 : std::max(last, shown) - shown;
    if(focused)
    {
        look.cursor = placeholder ? 0 : m_cursor - shown;
    }
    look.style = {placeholder && !focused ? theme.disabled_foreground : foreground_color, background_color};
    look.selected = {selection_fg, selection_bg};
    painter.draw_text_field(bounds(), look, theme);
    painter.draw_field_frame(bounds(), focused, theme);
}
std::pair<std::size_t, std::size_t> LineEdit::selection_range() const
{
    return {std::min(m_anchor, m_cursor), std::max(m_anchor, m_cursor)};
}
void LineEdit::replace_selection(std::string_view replacement)
{
    const auto [first, last]{selection_range()};
    m_text.replace(first, last - first, replacement);
    m_cursor = first + replacement.size();
    m_anchor = m_cursor;
    m_changed.fire(m_text);
}
bool LineEdit::erase_selection()
{
    const auto [first, last]{selection_range()};
    if(first == last)
    {
        return false;
    }
    replace_selection({});
    return true;
}
void LineEdit::move_left(bool extend)
{
    const auto [first, last]{selection_range()};
    m_cursor = first != last && !extend ? first : (m_cursor > 0 ? previous_codepoint(m_cursor) : 0);
    if(!extend)
    {
        m_anchor = m_cursor;
    }
}
void LineEdit::move_right(bool extend)
{
    const auto [first, last]{selection_range()};
    if(first != last && !extend)
    {
        m_cursor = last;
    }
    else if(m_cursor < m_text.size())
    {
        static_cast<void>(decode_utf8(m_text, m_cursor));
    }
    if(!extend)
    {
        m_anchor = m_cursor;
    }
}
void LineEdit::move_word(bool forward, bool extend)
{
    m_cursor = forward ? word_boundary::next(m_text, m_cursor) : word_boundary::previous(m_text, m_cursor);
    if(!extend)
    {
        m_anchor = m_cursor;
    }
}
std::size_t LineEdit::previous_codepoint(std::size_t index) const
{
    do
    {
        -- index;
    } while(index > 0 && (static_cast<unsigned char>(m_text.at(index)) & 0xc0) == 0x80);
    return index;
}
std::unique_ptr<LineEdit> line_edit()
{
    return std::make_unique<LineEdit>();
}
LineEdit& line_edit(Container& parent)
{
    return parent.add(line_edit());
}
} // namespace olagarro::oui
