#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

[[nodiscard]] int text_width(std::string_view text);
// The cells interface text takes as it shows, as text_metrics() measures it, with the padding it keeps from the
// cells' edges: its columns in a terminal; in a proportional font, fewer than its characters, rounded up to a whole
// cell. What a widget with such text measures, and what a painter fills behind it.
[[nodiscard]] int shown_width(std::string_view text);
// The text in lines that fit `width` cells as text_metrics() measures it, broken at the spaces between words;
// a word wider than a line is broken where it has to be. A line break in the text always breaks.
[[nodiscard]] std::vector<std::string> wrap_text(std::string_view text, int width);
// The text cut to fit `width` cells as text_metrics() measures it, with '~' marking that some was cut.
[[nodiscard]] std::string fit_text(std::string_view text, int width);

} // namespace olagarro::oui
