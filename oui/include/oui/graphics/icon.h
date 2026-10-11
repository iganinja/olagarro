#pragma once

#include <oui/color.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// A program's picture for the desktop to show beside its name, as in a task bar: `size` pixels a side, row by row,
// as 0xAARRGGBB, so it can have see-through parts.
struct WindowIcon
{
    int size{0};
    std::vector<std::uint32_t> pixels;
};

// A square with rounded corners in `background`, with `letters` in OUI's cell font in `foreground` across it, as
// "OC" for OUI Code: a program's own picture without a picture to carry.
[[nodiscard]] WindowIcon text_icon(std::string_view letters, Color background, Color foreground, int size = 128);

} // namespace olagarro::oui
