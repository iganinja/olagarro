// TextEdit: finding text, its matches and occurrences, and the bracket that pairs with another.
#include <oui/text_edit.h>

#include "word_boundary.h"

#include <algorithm>
#include <string>
#include <vector>

namespace olagarro::oui
{

bool TextEdit::find_next(std::string_view needle, const SearchOptions& options)
{
    drop_added_cursors();
    ChangeNotifier notifier{*this};
    const auto match{find_match(needle, true, options)};
    if(!match)
    {
        return false;
    }
    m_anchor = match->first;
    m_cursor = match->second;
    ensure_cursor_visible();
    return true;
}
bool TextEdit::find_previous(std::string_view needle, const SearchOptions& options)
{
    drop_added_cursors();
    ChangeNotifier notifier{*this};
    const auto match{find_match(needle, false, options)};
    if(!match)
    {
        return false;
    }
    m_anchor = match->first;
    m_cursor = match->second;
    ensure_cursor_visible();
    return true;
}
bool TextEdit::replace_next(std::string_view needle, std::string_view replacement, const SearchOptions& options)
{
    drop_added_cursors();
    if(m_read_only || !find_next(needle, options))
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    replace_selection(replacement);
    return true;
}
std::size_t TextEdit::replace_all(std::string_view needle, std::string_view replacement, const SearchOptions& options)
{
    drop_added_cursors();
    if(m_read_only)
    {
        return 0;
    }
    const auto matches{find_matches(needle, options)};
    if(matches.empty())
    {
        return 0;
    }
    ChangeNotifier notifier{*this};
    flush_pending_text_change();
    remember_change();
    std::string result;
    std::size_t copied{};
    std::size_t replaced{};
    // The cursor keeps its place in the text around it; inside a replaced match it ends up after the
    // replacement.
    auto cursor{m_cursor};
    for(const auto& [start, end] : matches)
    {
        if(start < copied)
        {
            continue;
        }
        result.append(m_text, copied, start - copied);
        result.append(replacement);
        copied = end;
        ++ replaced;
        if(m_cursor >= end)
        {
            cursor = cursor + replacement.size() - (end - start);
        }
        else if(m_cursor > start)
        {
            cursor = result.size();
        }
    }
    result.append(m_text, copied, std::string::npos);
    m_text = std::move(result);
    ++ m_revision;
    m_line_starts_dirty = true;
    invalidate_visual_lines();
    m_cursor = std::min(cursor, m_text.size());
    m_anchor = m_cursor;
    ensure_cursor_visible();
    return replaced;
}
// Every match of `needle` in order, overlapping ones included.
std::vector<std::pair<std::size_t, std::size_t>> TextEdit::find_matches(std::string_view needle,
                                                                        const SearchOptions& options) const
{
    return TextSearch{std::string{needle}, options}.find_all(m_text);
}
// The first match after the selection going forward, or the last one before it going backward, wrapping
// around the text.
std::optional<std::pair<std::size_t, std::size_t>> TextEdit::find_match(std::string_view needle, bool forward,
                                                                        const SearchOptions& options) const
{
    const auto matches{find_matches(needle, options)};
    if(matches.empty())
    {
        return std::nullopt;
    }
    const auto [first, last]{selection_range()};
    if(forward)
    {
        for(const auto& match : matches)
        {
            if(match.first >= last)
            {
                return match;
            }
        }
        return matches.front();
    }
    for(auto match{matches.rbegin()}; match != matches.rend(); ++ match)
    {
        if(match->second <= first && match->first < first)
        {
            return *match;
        }
    }
    return matches.back();
}
void TextEdit::set_search_highlight(std::string_view needle, const SearchOptions& options)
{
    m_search_highlight.emplace(std::string{needle}, options);
    m_search_matches_revision.reset();
}
void TextEdit::clear_search_highlight()
{
    m_search_highlight.reset();
    m_search_matches.clear();
    m_search_matches_revision.reset();
}
void TextEdit::set_occurrences_shown(bool shown)
{
    m_occurrences_shown = shown;
}
std::pair<std::size_t, std::size_t> TextEdit::word_at_cursor() const
{
    return word_boundary::run_at(m_text, m_cursor);
}
const std::vector<std::pair<std::size_t, std::size_t>>& TextEdit::occurrences() const
{
    // A selection worth looking for: within a line, not only blanks, and short enough to be a name or a phrase.
    constexpr std::size_t longest{200};
    auto needle{m_occurrences_shown ? selected_text() : std::string{}};
    if(needle.size() > longest || needle.find('\n') != std::string::npos ||
       needle.find_first_not_of(" \t\r") == std::string::npos)
    {
        needle.clear();
    }
    if(needle.empty())
    {
        m_occurrences.clear();
        m_occurrences_of.clear();
        m_occurrences_revision.reset();
    }
    else if(needle != m_occurrences_of || m_occurrences_revision != m_revision)
    {
        m_occurrences = find_matches(needle, SearchOptions{});
        m_occurrences_of = std::move(needle);
        m_occurrences_revision = m_revision;
    }
    return m_occurrences;
}
const std::vector<std::pair<std::size_t, std::size_t>>& TextEdit::search_matches() const
{
    if(!m_search_highlight)
    {
        m_search_matches.clear();
    }
    else if(m_search_matches_revision != m_revision)
    {
        m_search_matches = find_matches(m_search_highlight->first, m_search_highlight->second);
        m_search_matches_revision = m_revision;
    }
    return m_search_matches;
}
std::size_t TextEdit::search_match_count() const
{
    return search_matches().size();
}
std::optional<std::size_t> TextEdit::selected_search_match() const
{
    const auto& matches{search_matches()};
    const auto selection{selection_range()};
    const auto found{std::find(matches.begin(), matches.end(), selection)};
    if(found == matches.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - matches.begin()) + 1;
}
void TextEdit::set_bracket_pairs(std::vector<std::pair<char, char>> pairs)
{
    m_bracket_pairs = std::move(pairs);
    m_bracket_match.reset();
}
std::optional<std::size_t> TextEdit::find_pair(std::size_t offset) const
{
    const auto character{m_text[offset]};
    for(const auto& [open, close] : m_bracket_pairs)
    {
        if(character != open && character != close)
        {
            continue;
        }
        // Brackets are ASCII, and no byte of a longer UTF-8 character is, so bytes can be compared.
        const auto forward{character == open};
        std::size_t depth{};
        for(auto index{offset};;)
        {
            if(forward ? ++ index >= m_text.size() : index-- == 0)
            {
                return std::nullopt;
            }
            if(m_text[index] == character)
            {
                ++ depth;
            }
            else if(m_text[index] == (forward ? close : open))
            {
                if(depth == 0)
                {
                    return index;
                }
                -- depth;
            }
        }
    }
    return std::nullopt;
}
std::optional<std::pair<std::size_t, std::size_t>> TextEdit::matching_brackets() const
{
    if(m_bracket_match && m_bracket_match->revision == m_revision && m_bracket_match->cursor == m_cursor)
    {
        return m_bracket_match->brackets;
    }
    std::optional<std::pair<std::size_t, std::size_t>> brackets;
    for(const auto offset : {m_cursor, m_cursor - 1})
    {
        // The second candidate wraps around to the largest offset when the cursor is at the start.
        if(offset < m_text.size())
        {
            if(const auto pair{find_pair(offset)})
            {
                brackets = std::pair{offset, *pair};
                break;
            }
        }
    }
    m_bracket_match = BracketMatch{m_revision, m_cursor, brackets};
    return brackets;
}
bool TextEdit::go_to_matching_bracket()
{
    const auto brackets{matching_brackets()};
    if(!brackets)
    {
        return false;
    }
    ChangeNotifier notifier{*this};
    drop_added_cursors();
    m_cursor = brackets->second;
    m_anchor = m_cursor;
    ensure_cursor_visible();
    return true;
}

} // namespace olagarro::oui
