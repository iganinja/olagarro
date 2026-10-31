// TextEdit: the keys and the mouse, the cursors, and typing, erasing and moving through the text.
#include <oui/text_edit.h>

#include <oui/utf8.h>

#include "unicode_composition.h"
#include "word_boundary.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace olagarro::oui
{

void TextEdit::set_claims_typing(bool claims)
{
    m_claims_typing = claims;
}

bool TextEdit::claims_key(const Event& event) const
{
    if(!m_claims_typing)
    {
        return false;
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        return !text->control && !text->alt;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || key->alt)
    {
        return false;
    }
    switch(key->key)
    {
    // With Ctrl too: a word at a time, or the start and end of the text.
    case Key::backspace:
    case Key::delete_:
    case Key::left:
    case Key::right:
    case Key::up:
    case Key::down:
    case Key::home:
    case Key::end:
        return true;
    case Key::enter:
    case Key::space:
    case Key::tab:
    case Key::shift_tab:
    case Key::page_up:
    case Key::page_down:
        return !key->control;
    default:
        return false;
    }
}

bool TextEdit::handle_event(const Event& event)
{
    ChangeNotifier notifier{*this};
    return handle_input(event);
}
bool TextEdit::handle_input(const Event& event)
{
    if(!m_carets.empty() && !std::holds_alternative<MouseEvent>(event))
    {
        return handle_at_every_cursor(event);
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        m_last_home_was_at_column_zero = false;
        m_last_end_was_at_eol = false;
        flush_pending_text_change();
        if(mouse->wheel_delta != 0)
        {
            m_scroll_line = static_cast<std::size_t>(std::clamp(static_cast<int>(m_scroll_line) - mouse->wheel_delta, 0,
                                                                static_cast<int>(maximum_scroll_line())));
            return true;
        }
        if(handle_scrollbar(*mouse) || handle_horizontal_scrollbar(*mouse))
        {
            return true;
        }
        if((!mouse->pressed && !mouse->released && (!mouse->dragging || !m_mouse_selecting)) || mouse->x < bounds().x ||
           mouse->x >= bounds().x + bounds().width || mouse->y < bounds().y || mouse->y >= bounds().y + text_rows())
        {
            return false;
        }
        // The drag and release that follow a double or triple click leave its selection alone.
        if(m_word_or_line_selected && !mouse->pressed)
        {
            m_word_or_line_selected = !mouse->released;
            return true;
        }
        m_word_or_line_selected = false;
        if(mouse->pressed)
        {
            drop_added_cursors();
        }
        const auto& vlines{visual_lines()};
        const auto vline_index{
            std::min(m_scroll_line + static_cast<std::size_t>(whole(mouse->y - bounds().y)), vlines.size() - 1)};
        const auto& vl{vlines.at(vline_index)};
        const auto column{m_scroll_column +
                          static_cast<std::size_t>(std::max(0, whole(mouse->x - bounds().x) - gutter_width()))};
        const auto clicked_column{std::min(column, column_at_byte(vl.start_byte, vl.end_byte))};
        m_cursor = byte_at_column(vl.start_byte, clicked_column);
        if(mouse->pressed && mouse->clicks >= 2)
        {
            // A double click selects the word under the pointer, a triple click its whole line.
            if(mouse->clicks == 2)
            {
                std::tie(m_anchor, m_cursor) = word_boundary::run_at(m_text, m_cursor);
            }
            else
            {
                const auto previous_break{m_cursor == 0 ? std::string::npos : m_text.rfind('\n', m_cursor - 1)};
                m_anchor = previous_break == std::string::npos ? 0 : previous_break + 1;
                const auto next_break{m_text.find('\n', m_cursor)};
                m_cursor = next_break == std::string::npos ? m_text.size() : next_break + 1;
            }
            m_mouse_selecting = false;
            m_mouse_dragging = false;
            m_word_or_line_selected = true;
            ensure_cursor_visible();
            return true;
        }
        if(mouse->pressed)
        {
            // Shift keeps the anchor, so the click extends the selection.
            if(!mouse->shift)
            {
                m_anchor = m_cursor;
            }
            m_mouse_selecting = true;
            m_mouse_dragging = false;
        }
        if(mouse->dragging)
        {
            m_mouse_dragging = true;
        }
        if(mouse->released)
        {
            if(m_mouse_dragging)
            {
                if(m_cursor >= m_anchor && m_cursor < m_text.size())
                {
                    static_cast<void>(decode_utf8(m_text, m_cursor));
                }
                else if(m_cursor < m_anchor && m_anchor < m_text.size())
                {
                    static_cast<void>(decode_utf8(m_text, m_anchor));
                }
            }
            m_mouse_selecting = false;
            m_mouse_dragging = false;
        }
        ensure_cursor_visible();
        return true;
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        m_last_home_was_at_column_zero = false;
        m_last_end_was_at_eol = false;
        if(!text->control && !text->alt && m_read_only)
        {
            return true;
        }
        if(!text->control && !text->alt)
        {
            // A combining accent typed after its letter (NFD input) merges into the precomposed character.
            if(is_diacritic(text->codepoint) && m_cursor == m_anchor && m_cursor > 0)
            {
                const auto base_start{previous_codepoint(m_cursor)};
                auto base_end{base_start};
                if(const auto composed{compose(decode_utf8(m_text, base_end), text->codepoint)})
                {
                    m_anchor = base_start;
                    insert_text(encode_utf8(*composed));
                    return true;
                }
            }
            insert_text(encode_utf8(text->codepoint));
            return true;
        }
        return false;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr)
    {
        return false;
    }
    if(key->key != Key::home)
    {
        m_last_home_was_at_column_zero = false;
    }
    if(key->key != Key::end)
    {
        m_last_end_was_at_eol = false;
    }
    const auto edits{key->key == Key::enter || key->key == Key::tab || key->key == Key::shift_tab ||
                     key->key == Key::backspace || key->key == Key::delete_};
    if(edits && m_read_only)
    {
        // Tab and Shift+Tab, which would indent, go on to move the focus, as they do from any other widget.
        return key->key != Key::tab && key->key != Key::shift_tab;
    }
    switch(key->key)
    {
    case Key::enter:
    {
        // The new line starts with the indentation of the current one, up to where the cursor is.
        const auto first{selection_range().first};
        const auto previous_break{first == 0 ? std::string::npos : m_text.rfind('\n', first - 1)};
        const auto line_start{previous_break == std::string::npos ? 0 : previous_break + 1};
        auto indent_end{line_start};
        while(m_auto_indent && indent_end < first && (m_text.at(indent_end) == ' ' || m_text.at(indent_end) == '\t'))
        {
            ++ indent_end;
        }
        insert_text("\n" + m_text.substr(line_start, indent_end - line_start));
        return true;
    }
    case Key::space:
        if(!key->control && !key->alt)
        {
            insert_text(" ");
            return true;
        }
        return false;
    case Key::tab:
    {
        const auto [first, last]{selection_range()};
        if(m_text.find('\n', first) < last)
        {
            indent_lines(false);
        }
        else if(m_use_spaces_for_tab)
        {
            insert_text(std::string(static_cast<std::size_t>(m_tab_spaces), ' '));
        }
        else
        {
            insert_text("\t");
        }
        return true;
    }
    case Key::shift_tab:
        indent_lines(true);
        return true;
    case Key::backspace:
        flush_pending_text_change();
        return key->control ? erase_word(false) : erase_before_cursor();
    case Key::delete_:
        flush_pending_text_change();
        return key->control ? erase_word(true) : erase_after_cursor();
    case Key::left:
        flush_pending_text_change();
        if(key->control)
        {
            move_word(false, key->shift);
        }
        else
        {
            move_left(key->shift);
        }
        return true;
    case Key::right:
        flush_pending_text_change();
        if(key->control)
        {
            move_word(true, key->shift);
        }
        else
        {
            move_right(key->shift);
        }
        return true;
    case Key::up:
        flush_pending_text_change();
        move_vertical(-1, key->shift);
        return true;
    case Key::down:
        flush_pending_text_change();
        move_vertical(1, key->shift);
        return true;
    case Key::home:
        flush_pending_text_change();
        if(key->control)
        {
            m_last_home_was_at_column_zero = false;
            move_to_document_edge(false, key->shift);
        }
        else if(m_last_home_was_at_column_zero)
        {
            m_last_home_was_at_column_zero = false;
            move_to_first_non_whitespace(key->shift);
        }
        else
        {
            move_to_line_edge(false, key->shift);
            m_last_home_was_at_column_zero = true;
        }
        return true;
    case Key::end:
        flush_pending_text_change();
        if(key->control)
        {
            m_last_end_was_at_eol = false;
            move_to_document_edge(true, key->shift);
        }
        else if(m_last_end_was_at_eol)
        {
            m_last_end_was_at_eol = false;
            move_to_last_non_whitespace(key->shift);
        }
        else
        {
            move_to_line_edge(true, key->shift);
            m_last_end_was_at_eol = true;
        }
        return true;
    case Key::page_up:
        flush_pending_text_change();
        move_page_vertical(-1, key->shift);
        return true;
    case Key::page_down:
        flush_pending_text_change();
        move_page_vertical(1, key->shift);
        return true;
    default:
        return false;
    }
}

bool TextEdit::handle_at_every_cursor(const Event& event)
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        // Escape leaves one cursor; with one already, it is not the editor's and goes on to what holds it.
        if(key->key == Key::escape && !key->control && !key->alt && !key->shift)
        {
            if(m_carets.empty())
            {
                return false;
            }
            drop_added_cursors();
            return true;
        }
        // Indenting works on lines, not cursors: it acts at the first cursor alone.
        const auto [first, last]{selection_range()};
        if(key->key == Key::shift_tab || (key->key == Key::tab && m_text.find('\n', first) < last))
        {
            drop_added_cursors();
            return handle_input(event);
        }
    }
    auto handled{false};
    at_every_cursor([this, &event, &handled] { handled = handle_input(event) || handled; });
    return handled;
}
void TextEdit::at_every_cursor(const std::function<void()>& action)
{
    flush_pending_text_change();
    const auto before{snapshot()};
    const auto undo_depth{m_undo.size()};
    const auto home_was_at_column_zero{m_last_home_was_at_column_zero};
    const auto end_was_at_eol{m_last_end_was_at_eol};
    auto carets{m_carets};
    carets.push_back({m_cursor, m_anchor});
    const auto first_cursor{carets.size() - 1};
    std::vector<std::size_t> order(carets.size());
    for(std::size_t index{}; index < order.size(); ++ index)
    {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(),
              [&carets](std::size_t left, std::size_t right) {
                  return std::min(carets[left].cursor, carets[left].anchor) <
                         std::min(carets[right].cursor, carets[right].anchor);
              });
    // The action sees one cursor at a time.
    m_carets.clear();
    // How much the text before the next cursor grew or shrank.
    std::ptrdiff_t shift{};
    const auto moved{[&shift](std::size_t offset)
                     { return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + shift); }};
    for(const auto index : order)
    {
        m_cursor = moved(carets[index].cursor);
        m_anchor = moved(carets[index].anchor);
        m_last_home_was_at_column_zero = home_was_at_column_zero;
        m_last_end_was_at_eol = end_was_at_eol;
        const auto size_before{m_text.size()};
        action();
        flush_pending_text_change();
        shift += static_cast<std::ptrdiff_t>(m_text.size()) - static_cast<std::ptrdiff_t>(size_before);
        carets[index] = {m_cursor, m_anchor};
    }
    // One step to undo, which brings every cursor back.
    m_undo.erase(m_undo.begin() + static_cast<std::ptrdiff_t>(undo_depth), m_undo.end());
    if(m_text != before.text)
    {
        m_undo.push_back(before);
        m_redo.clear();
        m_is_modified = true;
    }
    m_cursor = carets[first_cursor].cursor;
    m_anchor = carets[first_cursor].anchor;
    carets.erase(carets.begin() + static_cast<std::ptrdiff_t>(first_cursor));
    m_carets = std::move(carets);
    merge_cursors();
    ensure_cursor_visible();
}
void TextEdit::merge_cursors()
{
    if(m_carets.empty())
    {
        return;
    }
    struct Placed
    {
        Caret caret;
        bool first;
    };
    std::vector<Placed> placed{{{m_cursor, m_anchor}, true}};
    for(const auto& caret : m_carets)
    {
        placed.push_back({caret, false});
    }
    const auto start{[](const Caret& caret) { return std::min(caret.cursor, caret.anchor); }};
    const auto end{[](const Caret& caret) { return std::max(caret.cursor, caret.anchor); }};
    std::stable_sort(placed.begin(), placed.end(), [&start](const Placed& left, const Placed& right)
                     { return start(left.caret) < start(right.caret); });
    std::vector<Placed> merged;
    for(const auto& next : placed)
    {
        if(!merged.empty())
        {
            auto& previous{merged.back()};
            const auto touching{start(next.caret) == end(previous.caret) &&
                                (start(next.caret) == end(next.caret) || start(previous.caret) == end(previous.caret))};
            if(start(next.caret) < end(previous.caret) || start(next.caret) == start(previous.caret) || touching)
            {
                // Overlapping selections become one, running forward.
                const auto union_end{std::max(end(previous.caret), end(next.caret))};
                const auto union_start{start(previous.caret)};
                previous.caret = union_start == union_end ? (next.first ? next.caret : previous.caret)
                                                          : Caret{union_end, union_start};
                previous.first = previous.first || next.first;
                continue;
            }
        }
        merged.push_back(next);
    }
    m_carets.clear();
    for(const auto& each : merged)
    {
        if(each.first)
        {
            m_cursor = each.caret.cursor;
            m_anchor = each.caret.anchor;
        }
        else
        {
            m_carets.push_back(each.caret);
        }
    }
}
void TextEdit::drop_added_cursors()
{
    m_carets.clear();
}
bool TextEdit::add_cursor_above()
{
    return add_cursor_vertically(-1);
}
bool TextEdit::add_cursor_below()
{
    return add_cursor_vertically(1);
}
bool TextEdit::add_cursor_vertically(int direction)
{
    auto from{m_cursor};
    for(const auto& caret : m_carets)
    {
        from = direction < 0 ? std::min(from, caret.cursor) : std::max(from, caret.cursor);
    }
    const auto& vlines{visual_lines()};
    const auto [row, column]{visual_position(from)};
    if((direction < 0 && row == 0) || (direction > 0 && row + 1 >= vlines.size()))
    {
        return false;
    }
    flush_pending_text_change();
    const auto target_row{direction < 0 ? row - 1 : row + 1};
    const auto& target{vlines.at(target_row)};
    const auto offset{
        byte_at_column(target.start_byte, std::min(column, column_at_byte(target.start_byte, target.end_byte)))};
    m_carets.push_back({offset, offset});
    merge_cursors();
    // The new cursor comes into view, as the first one does when it moves.
    if(target_row < m_scroll_line)
    {
        m_scroll_line = target_row;
    }
    else if(text_rows() > 0 && target_row >= m_scroll_line + static_cast<std::size_t>(text_rows()))
    {
        m_scroll_line = target_row + 1 - static_cast<std::size_t>(text_rows());
    }
    return true;
}
std::size_t TextEdit::cursor_count() const
{
    return m_carets.size() + 1;
}
std::vector<std::pair<std::size_t, std::size_t>> TextEdit::selections() const
{
    std::vector<std::pair<std::size_t, std::size_t>> result{{m_anchor, m_cursor}};
    for(const auto& caret : m_carets)
    {
        result.emplace_back(caret.anchor, caret.cursor);
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
              { return std::min(left.first, left.second) < std::min(right.first, right.second); });
    return result;
}
std::vector<std::pair<std::size_t, std::size_t>> TextEdit::selection_ranges() const
{
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    for(const auto& [anchor, cursor] : selections())
    {
        ranges.emplace_back(std::min(anchor, cursor), std::max(anchor, cursor));
    }
    return ranges;
}
void TextEdit::insert_text(std::string_view text)
{
    const auto now{std::chrono::steady_clock::now()};
    if(m_pending_text_change)
    {
        const auto elapsed{now - m_last_text_change_time};
        if(elapsed > std::chrono::milliseconds{300})
        {
            flush_pending_text_change();
        }
    }
    if(!m_pending_text_change)
    {
        m_pending_text_change = snapshot();
        m_redo.clear();
    }
    m_last_text_change_time = now;
    const auto [first, last]{selection_range()};
    m_text.replace(first, last - first, text);
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_cursor = first + text.size();
    m_anchor = m_cursor;
    m_is_modified = true;
    ensure_cursor_visible();
}
std::pair<std::size_t, std::size_t> TextEdit::selection_range() const
{
    return {std::min(m_anchor, m_cursor), std::max(m_anchor, m_cursor)};
}
void TextEdit::replace_selection(std::string_view replacement)
{
    flush_pending_text_change();
    remember_change();
    const auto [first, last]{selection_range()};
    m_text.replace(first, last - first, replacement);
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_cursor = first + replacement.size();
    m_anchor = m_cursor;
    ensure_cursor_visible();
}
void TextEdit::erase_selection()
{
    const auto [first, last]{selection_range()};
    if(first != last)
    {
        replace_selection({});
    }
}
bool TextEdit::erase_before_cursor()
{
    const auto [first, last]{selection_range()};
    if(first != last)
    {
        erase_selection();
        return true;
    }
    if(m_cursor == 0)
    {
        return false;
    }
    m_anchor = previous_codepoint(m_cursor);
    erase_selection();
    return true;
}
// Deletes to the word boundary without leaving the line; at the line's edge only the line break goes.
bool TextEdit::erase_word(bool forward)
{
    const auto [first, last]{selection_range()};
    if(first != last)
    {
        erase_selection();
        return true;
    }
    if(forward)
    {
        const auto line_end{std::min(m_text.find('\n', m_cursor), m_text.size())};
        if(m_cursor == m_text.size())
        {
            return false;
        }
        m_anchor = m_cursor == line_end ? m_cursor + 1 : std::min(word_boundary::next(m_text, m_cursor), line_end);
    }
    else
    {
        if(m_cursor == 0)
        {
            return false;
        }
        const auto previous_break{m_text.rfind('\n', m_cursor - 1)};
        const auto line_start{previous_break == std::string::npos ? 0 : previous_break + 1};
        m_anchor =
            m_cursor == line_start ? m_cursor - 1 : std::max(word_boundary::previous(m_text, m_cursor), line_start);
    }
    erase_selection();
    return true;
}
bool TextEdit::erase_after_cursor()
{
    const auto [first, last]{selection_range()};
    if(first != last)
    {
        erase_selection();
        return true;
    }
    if(m_cursor == m_text.size())
    {
        return false;
    }
    auto next{m_cursor};
    static_cast<void>(decode_utf8(m_text, next));
    m_anchor = next;
    erase_selection();
    return true;
}
void TextEdit::move_left(bool extend)
{
    const auto [first, last]{selection_range()};
    if(!extend && first != last)
    {
        m_cursor = first;
    }
    else
    {
        m_cursor = previous_codepoint(m_cursor);
    }
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_right(bool extend)
{
    const auto [first, last]{selection_range()};
    if(!extend && first != last)
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
    ensure_cursor_visible();
}
void TextEdit::move_word(bool forward, bool extend)
{
    m_cursor = forward ? word_boundary::next(m_text, m_cursor) : word_boundary::previous(m_text, m_cursor);
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_to_line_edge(bool end, bool extend)
{
    const auto [line, column]{cursor_position()};
    static_cast<void>(column);
    const auto& lines{line_starts()};
    m_cursor = end ? line_end(lines.at(line)) : lines.at(line);
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_to_first_non_whitespace(bool extend)
{
    const auto [line, column]{cursor_position()};
    static_cast<void>(column);
    const auto& lines{line_starts()};
    const auto start{lines.at(line)};
    const auto end{line_end(start)};
    auto target{start};
    while(target < end && (m_text.at(target) == ' ' || m_text.at(target) == '\t'))
    {
        ++ target;
    }
    m_cursor = target;
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_to_last_non_whitespace(bool extend)
{
    const auto [line, column]{cursor_position()};
    static_cast<void>(column);
    const auto& lines{line_starts()};
    const auto start{lines.at(line)};
    const auto eol{line_end(start)};
    if(eol == start)
    {
        if(!extend)
        {
            m_anchor = m_cursor;
        }
        return;
    }
    auto target{eol};
    while(target > start && (m_text.at(target - 1) == ' ' || m_text.at(target - 1) == '\t'))
    {
        -- target;
    }
    m_cursor = target;
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_to_document_edge(bool end, bool extend)
{
    m_cursor = end ? m_text.size() : 0;
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_vertical(int offset, bool extend)
{
    const auto& vlines{visual_lines()};
    if(vlines.empty())
    {
        return;
    }
    const auto [vis_row, vis_col]{cursor_visual_line()};
    if(offset < 0 && vis_row == 0)
    {
        m_cursor = 0;
        if(!extend)
        {
            m_anchor = m_cursor;
        }
        ensure_cursor_visible();
        return;
    }
    if(offset > 0 && vis_row >= vlines.size() - 1)
    {
        m_cursor = m_text.size();
        if(!extend)
        {
            m_anchor = m_cursor;
        }
        ensure_cursor_visible();
        return;
    }
    const auto target_row{static_cast<std::size_t>(static_cast<int>(vis_row) + offset)};
    const auto clamped{std::min(target_row, vlines.size() - 1)};
    const auto& target_vl{vlines.at(clamped)};
    const auto line_total_col{column_at_byte(target_vl.start_byte, target_vl.end_byte)};
    const auto col_to_use{std::min(vis_col, line_total_col)};
    m_cursor = byte_at_column(target_vl.start_byte, col_to_use);
    if(!extend)
    {
        m_anchor = m_cursor;
    }
    ensure_cursor_visible();
}
void TextEdit::move_page_vertical(int direction, bool extend)
{
    const auto& vlines{visual_lines()};
    if(vlines.empty() || direction == 0)
    {
        return;
    }

    const auto [vis_row, vis_col]{cursor_visual_line()};
    const auto viewport_height{static_cast<std::size_t>(text_rows())};
    const auto page_height{std::max(std::size_t{1}, viewport_height - 1)};
    const auto maximum_scroll{vlines.size() > viewport_height ? vlines.size() - viewport_height : std::size_t{0}};
    const auto cursor_row_in_viewport{vis_row > m_scroll_line ? vis_row - m_scroll_line : std::size_t{0}};

    std::size_t target_row{};
    if(direction < 0)
    {
        target_row = vis_row > page_height ? vis_row - page_height : std::size_t{0};
    }
    else
    {
        target_row = std::min(vis_row + page_height, vlines.size() - 1);
    }

    const auto& target_vl{vlines.at(target_row)};
    const auto target_width{column_at_byte(target_vl.start_byte, target_vl.end_byte)};
    m_cursor = byte_at_column(target_vl.start_byte, std::min(vis_col, target_width));
    if(!extend)
    {
        m_anchor = m_cursor;
    }

    const auto desired_scroll{target_row > cursor_row_in_viewport ? target_row - cursor_row_in_viewport
                                                                  : std::size_t{0}};
    m_scroll_line = std::min(desired_scroll, maximum_scroll);
    ensure_cursor_visible();
}
std::size_t TextEdit::previous_codepoint(std::size_t index) const
{
    if(index == 0)
    {
        return 0;
    }
    do
    {
        -- index;
    } while(index > 0 && (static_cast<unsigned char>(m_text.at(index)) & 0xc0) == 0x80);
    return index;
}

} // namespace olagarro::oui
