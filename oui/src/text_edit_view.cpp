// TextEdit: how the text lies on screen, wrapped and coloured, its scroll bars, and painting it.
#include <oui/text_edit.h>

#include <oui/utf8.h>

#include "unicode_composition.h"

#include <algorithm>
#include <string>
#include <vector>

namespace olagarro::oui
{

void TextEdit::set_syntax_highlighter(std::shared_ptr<const SyntaxHighlighter> highlighter)
{
    m_highlighter = std::move(highlighter);
    m_highlighted.clear();
    m_highlighted_through.reset();
}
void TextEdit::update_syntax(std::size_t line) const
{
    if(m_highlighter == nullptr)
    {
        return;
    }
    if(m_highlighted_through && m_highlighted_through->first == m_revision && m_highlighted_through->second >= line)
    {
        return;
    }
    const auto& starts{line_starts()};
    line = std::min(line, starts.size() - 1);
    for(std::size_t index{}; index <= line; ++ index)
    {
        const std::string_view text{
            std::string_view{m_text}.substr(starts[index], line_end(starts[index]) - starts[index])};
        const auto hash{std::hash<std::string_view>{}(text)};
        const SyntaxHighlighter::State state{index == 0 ? 0 : m_highlighted[index - 1].end};
        if(index < m_highlighted.size() && m_highlighted[index].text_hash == hash &&
           m_highlighted[index].start == state)
        {
            continue;
        }
        if(index >= m_highlighted.size())
        {
            m_highlighted.resize(index + 1);
        }
        auto& highlighted{m_highlighted[index]};
        highlighted.spans.clear();
        highlighted.text_hash = hash;
        highlighted.start = state;
        highlighted.end = m_highlighter->highlight_line(text, state, highlighted.spans);
    }
    m_highlighted_through = std::pair{m_revision, line};
}
const std::vector<SyntaxSpan>& TextEdit::line_syntax(std::size_t line) const
{
    static const std::vector<SyntaxSpan> none;
    if(m_highlighter == nullptr || line >= line_starts().size())
    {
        return none;
    }
    update_syntax(line);
    return m_highlighted[line].spans;
}
std::optional<Point> TextEdit::cell_of(std::size_t offset) const
{
    const auto [row, column]{visual_position(std::min(offset, m_text.size()))};
    if(row < m_scroll_line || row >= m_scroll_line + static_cast<std::size_t>(text_rows()) ||
       column < m_scroll_column || column >= m_scroll_column + static_cast<std::size_t>(content_width()))
    {
        return std::nullopt;
    }
    return Point{bounds().x + gutter_width() + static_cast<int>(column - m_scroll_column),
                 bounds().y + static_cast<int>(row - m_scroll_line)};
}
const std::vector<std::size_t>& TextEdit::line_starts() const
{
    if(!m_line_starts_dirty)
    {
        return m_line_starts;
    }

    m_line_starts = {0};
    for(std::size_t index{}; index < m_text.size(); ++ index)
    {
        if(m_text.at(index) == '\n')
        {
            m_line_starts.push_back(index + 1);
        }
    }
    m_line_starts_dirty = false;
    return m_line_starts;
}
std::size_t TextEdit::line_end(std::size_t start) const
{
    const auto newline{m_text.find('\n', start)};
    return newline == std::string::npos ? m_text.size() : newline;
}

void TextEdit::invalidate_visual_lines()
{
    m_content_columns.reset();
    m_visual_lines_dirty = true;
}

const std::vector<TextEdit::VisualLine>& TextEdit::visual_lines() const
{
    // Wrapped lines depend on the width, which changes with the editor's size: they are made again for a new one.
    const auto width{static_cast<std::size_t>(content_width())};
    if(!m_visual_lines_dirty && m_visual_lines_width == width)
    {
        return m_visual_lines;
    }
    m_visual_lines_width = width;
    m_content_columns.reset();
    m_visual_lines.clear();
    const auto& lines{line_starts()};
    if(!m_word_wrap_enabled || width == 0)
    {
        for(std::size_t i{}; i < lines.size(); ++ i)
        {
            m_visual_lines.push_back({i, lines.at(i), line_end(lines.at(i))});
        }
    }
    else
    {
        for(std::size_t i{}; i < lines.size(); ++ i)
        {
            const auto line_start{lines.at(i)};
            const auto line_last{line_end(line_start)};
            auto segment_start{line_start};
            std::size_t column{};
            auto last_space_byte{line_start};
            auto index{line_start};
            while(index < line_last)
            {
                const auto byte{index};
                const auto ch{m_text.at(index)};
                const auto char_width{char_cell_width(index, column)};
                static_cast<void>(decode_utf8(m_text, index));
                if(ch == ' ' || ch == '\t')
                {
                    last_space_byte = index;
                }
                column += char_width;
                if(column > width && byte > segment_start)
                {
                    auto break_byte{byte};
                    if(last_space_byte > segment_start)
                    {
                        break_byte = last_space_byte;
                    }
                    m_visual_lines.push_back({i, segment_start, break_byte});
                    segment_start = break_byte;
                    index = break_byte;
                    column = 0;
                    last_space_byte = segment_start;
                }
            }
            m_visual_lines.push_back({i, segment_start, line_last});
        }
    }
    m_visual_lines_dirty = false;
    return m_visual_lines;
}

std::pair<std::size_t, std::size_t> TextEdit::cursor_visual_line() const
{
    return visual_position(m_cursor);
}
std::pair<std::size_t, std::size_t> TextEdit::visual_position(std::size_t offset) const
{
    const auto& vlines{visual_lines()};
    for(std::size_t i{}; i < vlines.size(); ++ i)
    {
        const auto& line{vlines.at(i)};
        const auto continues_on_next_visual_line{i + 1 < vlines.size() && vlines.at(i + 1).start_byte == line.end_byte};
        const auto contains_offset{
            offset >= line.start_byte &&
            (offset < line.end_byte || (offset == line.end_byte && !continues_on_next_visual_line))};
        if(contains_offset)
        {
            return {i, column_at_byte(line.start_byte, offset)};
        }
    }
    if(vlines.empty())
    {
        return {0, 0};
    }
    return {vlines.size() - 1, 0};
}

std::pair<std::size_t, std::size_t> TextEdit::cursor_position() const
{
    const auto& lines{line_starts()};
    const auto iterator{std::upper_bound(lines.begin(), lines.end(), m_cursor)};
    const auto line{static_cast<std::size_t>(std::distance(lines.begin(), iterator) - 1)};
    return {line, column_at_byte(lines.at(line), m_cursor)};
}
std::size_t TextEdit::char_cell_width(std::size_t index, std::size_t column) const
{
    if(m_text.at(index) == '\t')
    {
        const auto tab_width{static_cast<std::size_t>(std::max(1, m_tab_display_width))};
        return tab_width - column % tab_width;
    }
    auto position{index};
    return static_cast<std::size_t>(std::max(0, terminal_cell_width(decode_utf8(m_text, position))));
}
std::size_t TextEdit::column_at_byte(std::size_t start, std::size_t target) const
{
    auto index{start};
    std::size_t column{};
    while(index < target)
    {
        column += char_cell_width(index, column);
        static_cast<void>(decode_utf8(m_text, index));
    }
    return column;
}
std::size_t TextEdit::byte_at_column(std::size_t start, std::size_t column) const
{
    auto index{start};
    std::size_t current{};
    const auto end{line_end(start)};
    while(index < end)
    {
        const auto character{index};
        const auto width{char_cell_width(index, current)};
        if(current + width > column)
        {
            return character;
        }
        current += width;
        static_cast<void>(decode_utf8(m_text, index));
    }
    return end;
}
void TextEdit::ensure_cursor_visible()
{
    // Before the editor has a size there is no view to scroll; it shows the cursor once it gets one.
    if(bounds().width <= 0 || bounds().height <= 0)
    {
        m_reveal_cursor_when_arranged = true;
        return;
    }
    const auto [vis_row, vis_col]{cursor_visual_line()};
    const auto height{static_cast<std::size_t>(text_rows())};
    const auto width{static_cast<std::size_t>(content_width())};
    if(vis_row < m_scroll_line)
    {
        m_scroll_line = vis_row;
    }
    else if(vis_row >= m_scroll_line + height)
    {
        m_scroll_line = vis_row - height + 1;
    }
    // Wrapped lines fit across: nothing is ever off to the side.
    if(m_word_wrap_enabled)
    {
        m_scroll_column = 0;
    }
    else if(vis_col < m_scroll_column)
    {
        m_scroll_column = vis_col;
    }
    else if(vis_col >= m_scroll_column + width)
    {
        m_scroll_column = vis_col - width + 1;
    }
}
void TextEdit::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(m_reveal_cursor_when_arranged && bounds.width > 0 && bounds.height > 0)
    {
        m_reveal_cursor_when_arranged = false;
        ensure_cursor_visible();
    }
}
int TextEdit::gutter_width() const
{
    return m_line_numbers_visible ? static_cast<int>(std::to_string(line_starts().size()).size()) + 1 : 0;
}
int TextEdit::content_width() const
{
    return std::max(1, bounds().width - gutter_width() - scrollbar_width());
}
int TextEdit::scrollbar_width() const
{
    return m_scrollbar_visible && bounds().width > gutter_width() + 1 ? 1 : 0;
}
int TextEdit::horizontal_scrollbar_height() const
{
    return m_scrollbar_visible && !m_word_wrap_enabled && bounds().height > 1 ? 1 : 0;
}
int TextEdit::text_rows() const
{
    return std::max(1, bounds().height - horizontal_scrollbar_height());
}
// The width in columns of the longest line, plus one for a cursor after its end; cached until the text or
// its wrapping changes.
std::size_t TextEdit::content_columns() const
{
    if(!m_content_columns)
    {
        std::size_t widest{};
        for(const auto& line : visual_lines())
        {
            widest = std::max(widest, column_at_byte(line.start_byte, line.end_byte));
        }
        m_content_columns = widest + 1;
    }
    return *m_content_columns;
}
std::size_t TextEdit::maximum_scroll_column() const
{
    const auto visible{static_cast<std::size_t>(content_width())};
    return content_columns() > visible ? content_columns() - visible : 0u;
}
int TextEdit::horizontal_thumb_width() const
{
    const auto visible{content_width()};
    const auto total{static_cast<int>(content_columns())};
    return total <= visible ? visible : std::max(1, (visible * visible + total / 2) / total);
}
int TextEdit::horizontal_thumb_left() const
{
    const auto range{content_width() - horizontal_thumb_width()};
    const auto maximum{static_cast<int>(maximum_scroll_column())};
    return maximum == 0 ? 0 : (static_cast<int>(m_scroll_column) * range + maximum / 2) / maximum;
}
// The horizontal bar works like the vertical one: the track pages, the thumb drags, the cursor stays.
bool TextEdit::handle_horizontal_scrollbar(const MouseEvent& mouse)
{
    if(m_horizontal_thumb_grab)
    {
        if(mouse.dragging)
        {
            const auto range{content_width() - horizontal_thumb_width()};
            const auto left{std::clamp(mouse.x - bounds().x - gutter_width() - *m_horizontal_thumb_grab, 0, range)};
            const auto maximum{static_cast<int>(maximum_scroll_column())};
            m_scroll_column = range == 0 ? 0 : static_cast<std::size_t>((left * maximum + range / 2) / range);
        }
        if(mouse.released)
        {
            m_horizontal_thumb_grab.reset();
        }
        return true;
    }
    const auto track_left{bounds().x + gutter_width()};
    if(horizontal_scrollbar_height() == 0 || mouse.y != bounds().y + text_rows() || mouse.x < bounds().x ||
       mouse.x >= bounds().x + bounds().width)
    {
        return false;
    }
    if(!mouse.pressed || mouse.x < track_left || mouse.x >= track_left + content_width())
    {
        return true;
    }
    const auto column{mouse.x - track_left};
    const auto left{horizontal_thumb_left()};
    const auto page{static_cast<std::size_t>(std::max(1, content_width() - 1))};
    if(column < left)
    {
        m_scroll_column = m_scroll_column > page ? m_scroll_column - page : 0;
    }
    else if(column >= left + horizontal_thumb_width())
    {
        m_scroll_column = std::min(m_scroll_column + page, maximum_scroll_column());
    }
    else
    {
        m_horizontal_thumb_grab = column - left;
    }
    return true;
}
std::size_t TextEdit::maximum_scroll_line() const
{
    const auto total{visual_lines().size()};
    const auto visible{static_cast<std::size_t>(text_rows())};
    return total > visible ? total - visible : 0u;
}
int TextEdit::thumb_height() const
{
    const auto total{static_cast<int>(visual_lines().size())};
    const auto height{text_rows()};
    return total <= height ? height : std::max(1, height * height / total);
}
int TextEdit::thumb_top() const
{
    const auto range{text_rows() - thumb_height()};
    const auto maximum{static_cast<int>(maximum_scroll_line())};
    return maximum == 0 ? 0 : (static_cast<int>(m_scroll_line) * range + maximum / 2) / maximum;
}
// Track clicks page the view and the thumb drags it; neither moves the text cursor.
bool TextEdit::handle_scrollbar(const MouseEvent& mouse)
{
    if(m_thumb_grab)
    {
        if(mouse.dragging)
        {
            const auto range{text_rows() - thumb_height()};
            const auto top{std::clamp(mouse.y - bounds().y - *m_thumb_grab, 0, range)};
            const auto maximum{static_cast<int>(maximum_scroll_line())};
            m_scroll_line = range == 0 ? 0 : static_cast<std::size_t>((top * maximum + range / 2) / range);
        }
        if(mouse.released)
        {
            m_thumb_grab.reset();
        }
        return true;
    }
    const auto track_x{bounds().x + bounds().width - 1};
    if(scrollbar_width() == 0 || mouse.x != track_x || mouse.y < bounds().y || mouse.y >= bounds().y + text_rows())
    {
        return false;
    }
    if(!mouse.pressed)
    {
        return true;
    }
    const auto row{mouse.y - bounds().y};
    const auto top{thumb_top()};
    const auto page{static_cast<std::size_t>(std::max(1, text_rows() - 1))};
    if(row < top)
    {
        m_scroll_line = m_scroll_line > page ? m_scroll_line - page : 0;
    }
    else if(row >= top + thumb_height())
    {
        m_scroll_line = std::min(m_scroll_line + page, maximum_scroll_line());
    }
    else
    {
        m_thumb_grab = row - top;
    }
    return true;
}

void TextEdit::paint_line(Painter& painter, const Theme& theme, std::size_t index, std::size_t end, int x, int y,
                          const std::vector<SyntaxSpan>& spans, std::size_t line_start) const
{
    std::size_t column{};
    // The first span that ends past this visual line's start; it moves on as the line is drawn.
    auto span{std::upper_bound(spans.begin(), spans.end(), index - line_start,
                               [](std::size_t offset, const SyntaxSpan& candidate) { return offset < candidate.end; })};
    const auto ranges{selection_ranges()};
    // The first selection that ends past the start of this line; it moves on as the line is drawn.
    auto range{std::upper_bound(ranges.begin(), ranges.end(), index,
                                [](std::size_t offset, const std::pair<std::size_t, std::size_t>& candidate)
                                { return offset < candidate.second; })};
    const auto& matches{search_matches()};
    const auto& others{occurrences()};
    const auto brackets{matching_brackets()};
    // The first match that ends past the start of this line; it moves on as the line is drawn.
    auto match{std::upper_bound(matches.begin(), matches.end(), index,
                                [](std::size_t offset, const std::pair<std::size_t, std::size_t>& candidate)
                                { return offset < candidate.second; })};
    auto other{std::upper_bound(others.begin(), others.end(), index,
                                [](std::size_t offset, const std::pair<std::size_t, std::size_t>& candidate)
                                { return offset < candidate.second; })};
    while(index < end)
    {
        const auto character{index};
        auto codepoint{decode_utf8(m_text, index)};
        const auto visible{static_cast<int>(char_cell_width(character, column))};
        // Combining accents stored after their letter (NFD text) are drawn precomposed in the letter's cell.
        while(visible > 0 && index < end)
        {
            auto following{index};
            const auto mark{decode_utf8(m_text, following)};
            const auto composed{is_diacritic(mark) ? compose(codepoint, mark) : std::nullopt};
            if(!composed)
            {
                break;
            }
            codepoint = *composed;
            index = following;
        }
        while(range != ranges.end() && range->second <= character)
        {
            ++ range;
        }
        const auto selected{range != ranges.end() && range->first <= character};
        while(match != matches.end() && match->second <= character)
        {
            ++ match;
        }
        const auto highlighted{!selected && match != matches.end() && match->first <= character};
        while(other != others.end() && other->second <= character)
        {
            ++ other;
        }
        const auto occurring{!selected && !highlighted && other != others.end() && other->first <= character};
        const auto bracket{!selected && !highlighted && !occurring && brackets &&
                           (character == brackets->first || character == brackets->second)};
        const auto background{selected      ? theme.selection_background
                              : highlighted ? theme.search_match_background
                              : occurring   ? theme.occurrence_background
                              : bracket     ? theme.bracket_match_background
                                            : theme.editor_background};
        while(span != spans.end() && span->end <= character - line_start)
        {
            ++ span;
        }
        const auto syntax{span != spans.end() && span->start <= character - line_start ? span->kind
                                                                                       : SyntaxKind::plain};
        const auto foreground{selected      ? theme.selection_foreground
                              : highlighted ? theme.search_match_foreground
                              : occurring   ? theme.occurrence_foreground
                              : bracket     ? theme.bracket_match_foreground
                                            : syntax_color(theme, syntax)};
        if((selected || highlighted || occurring || bracket) && visible > 0)
        {
            const auto start{std::max(static_cast<int>(column), static_cast<int>(m_scroll_column))};
            const auto finish{
                std::min(static_cast<int>(column) + visible, static_cast<int>(m_scroll_column) + content_width())};
            if(start < finish)
            {
                painter.fill_rect({x + start - static_cast<int>(m_scroll_column), y, finish - start, 1}, background);
            }
        }
        if(m_text.at(character) != '\t' && column + static_cast<std::size_t>(visible) > m_scroll_column &&
           column < m_scroll_column + static_cast<std::size_t>(content_width()))
        {
            painter.draw_text({x + static_cast<int>(column - m_scroll_column), y}, encode_utf8(codepoint),
                              {foreground, background});
        }
        column += static_cast<std::size_t>(visible);
    }
}
void TextEdit::paint(Painter& painter, const Theme& theme) const
{
    painter.fill_rect(bounds(), theme.editor_background);
    const auto& vlines{visual_lines()};
    const auto gutter{gutter_width()};
    // Colours down to the last line shown, at once rather than a row at a time.
    if(m_highlighter != nullptr && !vlines.empty())
    {
        update_syntax(
            vlines[std::min(m_scroll_line + static_cast<std::size_t>(std::max(text_rows(), 1)) - 1, vlines.size() - 1)]
                .logical_line);
    }
    for(int row{}; row < text_rows(); ++ row)
    {
        const auto vline_index{m_scroll_line + static_cast<std::size_t>(row)};
        if(vline_index >= vlines.size())
        {
            break;
        }
        const auto& vl{vlines.at(vline_index)};
        // A wrapped line has its number on its first row only.
        if(gutter > 0 && vl.start_byte == line_starts()[vl.logical_line])
        {
            const auto number{std::to_string(vl.logical_line + 1)};
            painter.draw_text({bounds().x + gutter - static_cast<int>(number.size()) - 1, bounds().y + row}, number,
                              {theme.line_number_foreground, theme.editor_background});
        }
        paint_line(painter, theme, vl.start_byte, vl.end_byte, bounds().x + gutter, bounds().y + row,
                   line_syntax(vl.logical_line), line_starts()[vl.logical_line]);
    }
    if(scrollbar_width() > 0)
    {
        const auto track_x{bounds().x + bounds().width - 1};
        painter.draw_scrollbar(
            {track_x, bounds().y, 1, text_rows()},
            {bounds().y + thumb_top(), thumb_height(), theme.scrollbar_background, theme.scrollbar_foreground}, theme);
    }
    if(horizontal_scrollbar_height() > 0)
    {
        const auto track_y{bounds().y + text_rows()};
        const auto track_left{bounds().x + gutter_width()};
        painter.draw_scrollbar({track_left, track_y, content_width(), 1},
                               {track_left + horizontal_thumb_left(), horizontal_thumb_width(),
                                theme.scrollbar_background, theme.scrollbar_foreground},
                               theme);
    }
    if(!is_focused())
    {
        return;
    }
    const auto paint_cursor{
        [&](std::size_t offset)
        {
            const auto [vis_row, vis_col]{visual_position(offset)};
            if(vis_row < m_scroll_line || vis_row >= m_scroll_line + static_cast<std::size_t>(text_rows()) ||
               vis_col < m_scroll_column || vis_col >= m_scroll_column + static_cast<std::size_t>(content_width()))
            {
                return;
            }
            const Point cursor_point{bounds().x + gutter + static_cast<int>(vis_col - m_scroll_column),
                                     bounds().y + static_cast<int>(vis_row - m_scroll_line)};
            auto cursor_index{offset};
            const auto under_cursor{offset < m_text.size() ? m_text.at(offset) : '\n'};
            const auto cursor_char{
                under_cursor == '\n' || under_cursor == '\t' ? " " : encode_utf8(decode_utf8(m_text, cursor_index))};
            painter.draw_text_cursor(cursor_point, cursor_char, {theme.editor_foreground, theme.editor_background});
        }};
    paint_cursor(m_cursor);
    for(const auto& caret : m_carets)
    {
        paint_cursor(caret.cursor);
    }
}

} // namespace olagarro::oui
