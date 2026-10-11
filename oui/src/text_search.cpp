#include <oui/text_search.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] bool is_word_byte(char value)
{
    const auto byte{static_cast<unsigned char>(value)};
    return byte == '_' || byte >= 0x80 || std::isalnum(byte) != 0;
}

[[nodiscard]] std::string fold_case(std::string_view text)
{
    std::string folded{text};
    std::transform(folded.begin(), folded.end(), folded.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return folded;
}

} // namespace

TextSearch::TextSearch(std::string needle, SearchOptions options)
    : m_needle{std::move(needle)}, m_options{options}, m_valid{!m_needle.empty()}
{
    if(!m_valid)
    {
        return;
    }
    if(m_options.regex)
    {
        try
        {
            m_expression.emplace(m_needle, m_options.match_case ? "" : "i");
        }
        catch(const std::invalid_argument&)
        {
            m_valid = false;
        }
    }
    else if(!m_options.match_case)
    {
        m_needle = fold_case(m_needle);
    }
}

bool TextSearch::is_valid() const
{
    return m_valid;
}

std::vector<std::pair<std::size_t, std::size_t>> TextSearch::find_all(std::string_view text) const
{
    std::vector<std::pair<std::size_t, std::size_t>> matches;
    if(!m_valid)
    {
        return matches;
    }
    const auto whole_word{[&text](std::size_t start, std::size_t end) {
        return (start == 0 || !is_word_byte(text[start - 1])) && (end >= text.size() || !is_word_byte(text[end]));
    }};
    if(m_expression)
    {
        // The engine searches UTF-16, as JavaScript does; each match is read back as the bytes it covers. An
        // empty match is left out, and the search goes on after it.
        const Utf16Text units{text};
        for(std::size_t from{}; from <= units.units.size();)
        {
            const auto found{m_expression->search(units.units, from)};
            if(!found)
            {
                break;
            }
            const auto [first, last]{*found};
            if(last == first)
            {
                from = first + 1;
                continue;
            }
            const auto start{units.byte_offsets[first]};
            const auto end{units.byte_offsets[last]};
            if(!m_options.whole_word || whole_word(start, end))
            {
                matches.emplace_back(start, end);
            }
            from = last;
        }
        return matches;
    }
    const auto folded{m_options.match_case ? std::string{} : fold_case(text)};
    const std::string_view haystack{m_options.match_case ? text : std::string_view{folded}};
    for(auto found{haystack.find(m_needle)}; found != std::string_view::npos;
        found = haystack.find(m_needle, found + 1))
    {
        if(!m_options.whole_word || whole_word(found, found + m_needle.size()))
        {
            matches.emplace_back(found, found + m_needle.size());
        }
    }
    return matches;
}

} // namespace olagarro::oui
