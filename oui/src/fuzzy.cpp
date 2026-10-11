#include <oui/fuzzy.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>

namespace olagarro::oui
{

namespace
{

constexpr int match_points{16};
constexpr int word_start_points{10};
constexpr int together_points{12};
constexpr int file_name_points{4};
constexpr int gap_points{1};
constexpr int unmatched{std::numeric_limits<int>::min() / 2};

[[nodiscard]] char folded(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool separator(char c)
{
    return c == '/' || c == '\\' || c == '_' || c == '-' || c == '.' || c == ' ';
}

// A position where a word starts: the text's start, after a separator, or a capital after a lowercase letter.
[[nodiscard]] bool starts_word(std::string_view text, std::size_t index)
{
    if(index == 0)
    {
        return true;
    }
    const auto previous{text[index - 1]};
    return separator(previous) || (std::islower(static_cast<unsigned char>(previous)) != 0 &&
                                   std::isupper(static_cast<unsigned char>(text[index])) != 0);
}

} // namespace

std::optional<int> fuzzy_score(std::string_view pattern, std::string_view text)
{
    if(pattern.empty())
    {
        return 0;
    }
    if(pattern.size() > text.size())
    {
        return std::nullopt;
    }
    const auto name_start{text.find_last_of("/\\") == std::string_view::npos ? 0 : text.find_last_of("/\\") + 1};
    // best[j]: the best score with the pattern so far matched and its last character at text position j.
    std::vector<int> previous(text.size(), unmatched);
    std::vector<int> current(text.size(), unmatched);
    for(std::size_t i{}; i < pattern.size(); ++ i)
    {
        const auto wanted{folded(pattern[i])};
        // The best way to have matched the characters before this one somewhere before position j, gaps paid.
        auto carried{i == 0 ? 0 : unmatched};
        for(std::size_t j{}; j < text.size(); ++ j)
        {
            current[j] = unmatched;
            if(folded(text[j]) == wanted && carried != unmatched)
            {
                auto points{match_points + (starts_word(text, j) ? word_start_points : 0) +
                            (j >= name_start ? file_name_points : 0)};
                auto best{carried + points};
                if(i > 0 && j > 0 && previous[j - 1] != unmatched)
                {
                    best = std::max(best, previous[j - 1] + points + together_points);
                }
                current[j] = best;
            }
            if(i > 0)
            {
                // Position j becomes a place the previous character may have matched, one gap further each step.
                carried = std::max(carried == unmatched ? unmatched : carried - gap_points, previous[j]);
            }
        }
        std::swap(previous, current);
    }
    const auto best{*std::max_element(previous.begin(), previous.end())};
    if(best == unmatched)
    {
        return std::nullopt;
    }
    return best;
}

bool wildcard_match(std::string_view pattern, std::string_view text)
{
    std::size_t p{};
    std::size_t t{};
    // Where the last '*' was, and where in the text it started matching, to backtrack to.
    std::optional<std::size_t> star;
    std::size_t star_text{};
    while(t < text.size())
    {
        if(p < pattern.size() && (pattern[p] == '?' || folded(pattern[p]) == folded(text[t])))
        {
            ++ p;
            ++ t;
        }
        else if(p < pattern.size() && pattern[p] == '*')
        {
            star = p++;
            star_text = t;
        }
        else if(star)
        {
            p = *star + 1;
            t = ++ star_text;
        }
        else
        {
            return false;
        }
    }
    while(p < pattern.size() && pattern[p] == '*')
    {
        ++ p;
    }
    return p == pattern.size();
}

} // namespace olagarro::oui
