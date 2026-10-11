#pragma once

#include <optional>
#include <string_view>

namespace olagarro::oui
{

// How well `pattern` matches `text` as the fuzzy search of code editors does: its characters in order, not
// necessarily together, ignoring case. Nothing when they are not all there. Otherwise higher is better:
// characters that come together, that start words, or that fall in the last part of a path count most, and
// gaps between them count against.
[[nodiscard]] std::optional<int> fuzzy_score(std::string_view pattern, std::string_view text);

// Whether `text` matches the wildcard `pattern` as a whole, ignoring case: '*' stands for any run of
// characters, slashes included, and '?' for any one.
[[nodiscard]] bool wildcard_match(std::string_view pattern, std::string_view text);

} // namespace olagarro::oui
