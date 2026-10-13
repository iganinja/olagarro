#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// A picture in pixels, row by row, as 0xAARRGGBB with straight alpha: its colour is whole however see-through it is.
struct Picture
{
    int width{0};
    int height{0};
    std::vector<std::uint32_t> pixels;
};

// An SVG drawn `width` by `height` pixels, as big as fits keeping its proportions and in the middle; none when `svg`
// is not a picture NanoSVG can read.
[[nodiscard]] std::optional<Picture> draw_svg(std::string_view svg, int width, int height);

} // namespace olagarro::oui
