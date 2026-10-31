// TextEdit: its text and state, what is selected and the clipboard, undo and redo. The rest of it is in
// text_edit_input.cpp, text_edit_lines.cpp, text_edit_search.cpp and text_edit_view.cpp.
#include <oui/text_edit.h>

#include <oui/utf8.h>

#include <algorithm>
#include <vector>

namespace olagarro::oui
{

void TextEdit::set_text(std::string_view text)
{
    ChangeNotifier notifier{*this};
    ++ m_revision;
    m_text = text;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_cursor = 0;
    m_anchor = 0;
    m_carets.clear();
    m_scroll_line = 0;
    m_scroll_column = 0;
    m_undo.clear();
    m_redo.clear();
    m_pending_text_change.reset();
    m_is_modified = false;
}
std::string_view TextEdit::text() const
{
    return m_text;
}
bool TextEdit::is_modified() const
{
    return m_is_modified;
}
std::size_t TextEdit::revision() const
{
    return m_revision;
}
void TextEdit::set_line_numbers_visible(bool visible)
{
    m_line_numbers_visible = visible;
}

void TextEdit::set_tab_spaces(int spaces)
{
    if(m_tab_spaces == spaces)
    {
        return;
    }
    m_tab_spaces = spaces > 0 ? spaces : 4;
    invalidate_visual_lines();
}

void TextEdit::set_use_spaces(bool use)
{
    m_use_spaces_for_tab = use;
}

void TextEdit::set_scrollbar_visible(bool visible)
{
    m_scrollbar_visible = visible;
    invalidate_visual_lines();
}
void TextEdit::set_word_wrap(bool wrap)
{
    m_word_wrap_enabled = wrap;
    if(wrap)
    {
        m_scroll_column = 0;
    }
    invalidate_visual_lines();
}

bool TextEdit::word_wrap() const
{
    return m_word_wrap_enabled;
}

std::pair<std::size_t, std::size_t> TextEdit::line_column() const
{
    const auto [line, column]{cursor_position()};
    return {line + 1, column + 1};
}
void TextEdit::mark_saved()
{
    m_is_modified = false;
}
void TextEdit::mark_modified()
{
    m_is_modified = true;
}
void TextEdit::set_tab_display_width(int columns)
{
    m_tab_display_width = std::max(1, columns);
    invalidate_visual_lines();
}
void TextEdit::set_auto_indent(bool enabled)
{
    m_auto_indent = enabled;
}
void TextEdit::set_read_only(bool read_only)
{
    m_read_only = read_only;
}
bool TextEdit::is_read_only() const
{
    return m_read_only;
}
TextEdit::ChangeNotifier::ChangeNotifier(TextEdit& editor)
    : m_editor{editor}, m_revision{editor.m_revision}, m_cursor{editor.m_cursor}, m_anchor{editor.m_anchor}
{
}
TextEdit::ChangeNotifier::~ChangeNotifier()
{
    if(m_editor.m_revision != m_revision)
    {
        m_editor.m_changed.fire();
    }
    if(m_editor.m_cursor != m_cursor || m_editor.m_anchor != m_anchor)
    {
        m_editor.m_cursor_moved.fire();
    }
}
bool TextEdit::is_focusable() const
{
    return true;
}
std::string TextEdit::selected_text() const
{
    const auto [first, last]{selection_range()};
    return m_text.substr(first, last - first);
}
void TextEdit::select_all()
{
    drop_added_cursors();
    m_anchor = 0;
    m_cursor = m_text.size();
    ensure_cursor_visible();
}
// With several cursors, their selections one per line, in the order of the text.
void TextEdit::copy_selection(std::string& clipboard) const
{
    clipboard.clear();
    const auto ranges{selection_ranges()};
    for(std::size_t index{}; index < ranges.size(); ++ index)
    {
        clipboard +=
            (index == 0 ? "" : "\n") + m_text.substr(ranges[index].first, ranges[index].second - ranges[index].first);
    }
}
void TextEdit::cut_selection(std::string& clipboard)
{
    copy_selection(clipboard);
    if(!m_read_only)
    {
        ChangeNotifier notifier{*this};
        at_every_cursor([this] { erase_selection(); });
    }
}
// With several cursors and as many lines as cursors, each cursor gets its line, as a copy from as many
// cursors put them; otherwise each gets the whole text.
void TextEdit::paste(std::string_view text)
{
    if(m_read_only)
    {
        return;
    }
    ChangeNotifier notifier{*this};
    std::vector<std::string_view> pieces;
    if(!m_carets.empty())
    {
        for(std::size_t start{};;)
        {
            const auto end{text.find('\n', start)};
            pieces.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            if(end == std::string_view::npos)
            {
                break;
            }
            start = end + 1;
        }
    }
    if(pieces.size() != cursor_count())
    {
        pieces.assign(cursor_count(), text);
    }
    std::size_t next{};
    at_every_cursor([this, &pieces, &next] { replace_selection(pieces[next++]); });
}
bool TextEdit::go_to_line(std::size_t number)
{
    const auto& lines{line_starts()};
    if(number == 0 || number > lines.size())
    {
        return false;
    }
    drop_added_cursors();
    m_cursor = lines.at(number - 1);
    m_anchor = m_cursor;
    ensure_cursor_visible();
    return true;
}
bool TextEdit::undo()
{
    if(m_read_only)
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    flush_pending_text_change();
    if(m_undo.empty())
    {
        return false;
    }
    m_redo.push_back(snapshot());
    restore(std::move(m_undo.back()));
    m_undo.pop_back();
    return true;
}
bool TextEdit::redo()
{
    if(m_read_only)
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    flush_pending_text_change();
    if(m_redo.empty())
    {
        return false;
    }
    m_undo.push_back(snapshot());
    restore(std::move(m_redo.back()));
    m_redo.pop_back();
    return true;
}
Size TextEdit::measure(const Constraints& constraints) const
{
    return constraints.constrain({std::max(1.0, constraints.max_width), std::max(1.0, constraints.max_height)});
}
TextEdit::Snapshot TextEdit::snapshot() const
{
    return {m_text, m_cursor, m_anchor, m_is_modified, m_carets};
}
void TextEdit::restore(Snapshot state)
{
    m_text = std::move(state.text);
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_cursor = state.cursor;
    m_anchor = state.anchor;
    m_is_modified = state.is_modified;
    m_carets = std::move(state.carets);
    ensure_cursor_visible();
}
void TextEdit::remember_change()
{
    m_undo.push_back(snapshot());
    m_redo.clear();
    m_is_modified = true;
}
void TextEdit::flush_pending_text_change()
{
    if(!m_pending_text_change)
    {
        return;
    }
    m_undo.push_back(std::move(*m_pending_text_change));
    m_pending_text_change.reset();
    m_redo.clear();
    m_is_modified = true;
}

std::size_t TextEdit::cursor_offset() const
{
    return m_cursor;
}
void TextEdit::select_range(std::size_t start, std::size_t end)
{
    drop_added_cursors();
    m_anchor = std::min(start, m_text.size());
    m_cursor = std::min(end, m_text.size());
    ensure_cursor_visible();
}
std::unique_ptr<TextEdit> text_edit()
{
    return std::make_unique<TextEdit>();
}
TextEdit& text_edit(Container& parent)
{
    return parent.add(text_edit());
}

} // namespace olagarro::oui
