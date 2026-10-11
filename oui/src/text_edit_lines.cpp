// TextEdit: what works on whole lines: duplicating, moving, deleting, commenting and indenting them.
#include <oui/text_edit.h>

#include <algorithm>
#include <string>
#include <vector>

namespace olagarro::oui
{

std::pair<std::size_t, std::size_t> TextEdit::selected_lines() const
{
    const auto [first, last]{selection_range()};
    const auto& starts{line_starts()};
    const auto line_of{[&starts](std::size_t offset) {
        return static_cast<std::size_t>(std::upper_bound(starts.begin(), starts.end(), offset) - starts.begin()) - 1;
    }};
    const auto first_line{line_of(first)};
    auto last_line{line_of(last)};
    // A selection that ends at the start of a line does not take that line in.
    if(last_line > first_line && starts.at(last_line) == last)
    {
        -- last_line;
    }
    return {first_line, last_line};
}
void TextEdit::replace_lines(std::size_t start, std::size_t end, std::string_view text, std::size_t anchor,
                             std::size_t cursor)
{
    flush_pending_text_change();
    remember_change();
    m_text.replace(start, end - start, text);
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_anchor = std::min(anchor, m_text.size());
    m_cursor = std::min(cursor, m_text.size());
    ensure_cursor_visible();
}
bool TextEdit::duplicate_lines()
{
    drop_added_cursors();
    if(m_read_only)
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    const auto [first_line, last_line]{selected_lines()};
    const auto start{line_starts().at(first_line)};
    const auto end{line_end(line_starts().at(last_line))};
    const auto block{m_text.substr(start, end - start)};
    // The copy goes below, and the selection with it, so repeating it keeps copying downwards.
    const auto shift{block.size() + 1};
    replace_lines(end, end, "\n" + block, m_anchor + shift, m_cursor + shift);
    return true;
}
bool TextEdit::move_lines(bool down)
{
    drop_added_cursors();
    if(m_read_only)
    {
        return false;
    }
    const auto [first_line, last_line]{selected_lines()};
    const auto start{line_starts().at(first_line)};
    const auto end{line_end(line_starts().at(last_line))};
    const auto block{m_text.substr(start, end - start)};
    if(!down)
    {
        if(first_line == 0)
        {
            return false;
        }
        ChangeNotifier notifier{*this};
        const auto above{line_starts().at(first_line - 1)};
        const auto previous{m_text.substr(above, start - 1 - above)};
        const auto shift{previous.size() + 1};
        replace_lines(above, end, block + "\n" + previous, m_anchor - shift, m_cursor - shift);
        return true;
    }
    if(end >= m_text.size())
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    const auto next_end{line_end(end + 1)};
    const auto next{m_text.substr(end + 1, next_end - end - 1)};
    const auto shift{next.size() + 1};
    replace_lines(start, next_end, next + "\n" + block, m_anchor + shift, m_cursor + shift);
    return true;
}
bool TextEdit::delete_lines()
{
    drop_added_cursors();
    if(m_read_only || m_text.empty())
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    const auto [first_line, last_line]{selected_lines()};
    auto start{line_starts().at(first_line)};
    auto end{line_end(line_starts().at(last_line))};
    // The lines go with one line break: the one after them, and the cursor lands on the line that moves up
    // into their place; for the last lines, the one before, and the cursor lands on the line above.
    auto cursor{start};
    if(end < m_text.size())
    {
        ++ end;
    }
    else if(start > 0)
    {
        cursor = line_starts().at(first_line - 1);
        -- start;
    }
    replace_lines(start, end, {}, cursor, cursor);
    return true;
}
bool TextEdit::toggle_line_comment(std::string_view prefix)
{
    drop_added_cursors();
    if(m_read_only || prefix.empty())
    {
        return false;
    }
    const auto [first_line, last_line]{selected_lines()};
    const auto& starts{line_starts()};
    const auto block_start{starts.at(first_line)};
    const auto block_end{line_end(starts.at(last_line))};
    std::string text{m_text.substr(block_start, block_end - block_start)};
    // Lines as offsets within `text`, with where their content starts after the indentation.
    struct Line
    {
        std::size_t start;
        std::size_t content;
        std::size_t end;
    };
    std::vector<Line> lines;
    for(std::size_t start{};;)
    {
        const auto newline{text.find('\n', start)};
        const auto end{newline == std::string::npos ? text.size() : newline};
        lines.push_back({start, text.find_first_not_of(" \t", start), end});
        if(newline == std::string::npos)
        {
            break;
        }
        start = newline + 1;
    }
    const auto blank{[](const Line& line) { return line.content == std::string::npos || line.content >= line.end; }};
    // Commented when every line with text starts with the prefix; blank lines do not count either way.
    auto any_text{false};
    auto all_commented{true};
    std::size_t indentation{std::string::npos};
    for(const auto& line : lines)
    {
        if(blank(line))
        {
            continue;
        }
        any_text = true;
        all_commented = all_commented && text.compare(line.content, prefix.size(), prefix) == 0;
        indentation = std::min(indentation, line.content - line.start);
    }
    if(!any_text)
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    auto anchor{m_anchor};
    auto cursor{m_cursor};
    // Moves the selection's ends past text inserted at `at`, or back over text removed from there.
    const auto inserted_at{[&anchor, &cursor](std::size_t at, std::size_t length)
                           {
                               for(auto* const offset : {&anchor, &cursor})
                               {
                                   if(*offset > at)
                                   {
                                       *offset += length;
                                   }
                               }
                           }};
    const auto removed_at{[&anchor, &cursor](std::size_t at, std::size_t length)
                          {
                              for(auto* const offset : {&anchor, &cursor})
                              {
                                  if(*offset > at)
                                  {
                                      *offset = std::max(at, *offset - length);
                                  }
                              }
                          }};
    // From the last line up, so the offsets of the lines still to do stay valid.
    for(auto line{lines.rbegin()}; line != lines.rend(); ++ line)
    {
        if(blank(*line))
        {
            continue;
        }
        if(all_commented)
        {
            auto length{prefix.size()};
            if(line->content + length < line->end && text.at(line->content + length) == ' ')
            {
                ++ length;
            }
            text.erase(line->content, length);
            removed_at(block_start + line->content, length);
        }
        else
        {
            const auto at{line->start + indentation};
            const auto inserted{std::string{prefix} + " "};
            text.insert(at, inserted);
            inserted_at(block_start + at, inserted.size());
        }
    }
    replace_lines(block_start, block_end, text, anchor, cursor);
    return true;
}
void TextEdit::indent_lines(bool outdent)
{
    flush_pending_text_change();
    const auto [first_line, last_line]{selected_lines()};
    const auto& starts{line_starts()};
    const std::vector<std::size_t> lines(starts.begin() + static_cast<std::ptrdiff_t>(first_line),
                                         starts.begin() + static_cast<std::ptrdiff_t>(last_line) + 1);
    const std::string unit{m_use_spaces_for_tab ? std::string(static_cast<std::size_t>(m_tab_spaces), ' ') : "\t"};
    auto anchor{m_anchor};
    auto cursor{m_cursor};
    auto changed{false};
    // From the last line up, so the offsets of the lines still to do stay valid.
    for(auto line{lines.rbegin()}; line != lines.rend(); ++ line)
    {
        const auto start{*line};
        const auto end{line_end(start)};
        std::size_t count{};
        if(!outdent)
        {
            if(start == end)
            {
                continue;
            }
            if(!changed)
            {
                remember_change();
            }
            m_text.insert(start, unit);
            changed = true;
            count = unit.size();
            for(auto* const offset : {&anchor, &cursor})
            {
                if(*offset > start)
                {
                    *offset += count;
                }
            }
            continue;
        }
        if(!m_use_spaces_for_tab && start < end && m_text.at(start) == '\t')
        {
            count = 1;
        }
        else
        {
            while(count < static_cast<std::size_t>(m_tab_spaces) && start + count < end &&
                  (m_text.at(start + count) == ' ' || m_text.at(start + count) == '\t'))
            {
                ++ count;
            }
        }
        if(count == 0)
        {
            continue;
        }
        if(!changed)
        {
            remember_change();
        }
        m_text.erase(start, count);
        changed = true;
        for(auto* const offset : {&anchor, &cursor})
        {
            *offset = *offset >= start + count ? *offset - count : std::min(*offset, start);
        }
    }
    if(!changed)
    {
        return;
    }
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_anchor = anchor;
    m_cursor = cursor;
    ensure_cursor_visible();
}

} // namespace olagarro::oui
