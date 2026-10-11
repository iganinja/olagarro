#pragma once

#include <oui/regex.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

struct SearchOptions
{
    bool match_case{true};
    // Only matches with no letter, digit or underscore right before or after them.
    bool whole_word{false};
    // JavaScript regular expressions, as RegExp reads them; an invalid expression finds nothing.
    bool regex{false};
};

// What to look for in UTF-8 text, prepared once to be looked for in as many texts as needed, such as every
// file of a project. Case folding is ASCII only, so matches keep the byte offsets of the text searched.
class TextSearch
{
  public:
    TextSearch(std::string needle, SearchOptions options = {});

    // False for an empty needle, or a regular expression that does not compile; such a search finds nothing.
    [[nodiscard]] bool is_valid() const;
    // Every match in `text`, as [start, end) byte offsets in order. Regular expression matches that are
    // empty are left out.
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> find_all(std::string_view text) const;

  private:
    std::string m_needle;
    SearchOptions m_options;
    std::optional<Regex> m_expression;
    bool m_valid{false};
};

} // namespace olagarro::oui
