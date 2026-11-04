#pragma once

#include <paint/color.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::paint
{

// A palette to paint with, as a colour panel shows it under the primary and secondary colours: its name and its
// colours in their order.
struct Palette
{
    std::string name;
    std::vector<Color> colors;

    [[nodiscard]] bool operator==(const Palette& other) const { return name == other.name && colors == other.colors; }
    [[nodiscard]] bool operator!=(const Palette& other) const { return !(*this == other); }

    // Greys from black to white, see-through, then the colours of the wheel and four dark ones: "Default".
    [[nodiscard]] static Palette standard();
};

// The palette as GIMP's .gpl text: a "GIMP Palette" line, its name, then a line "r g b<tab>name" a colour, 0 to 255.
// The format has no alpha: a colour not solid is named by its "#rrggbbaa", which gives its alpha back when read, and
// one wholly see-through is written as black named "transparent".
[[nodiscard]] std::string to_gpl(const Palette& palette);
// A palette from .gpl text, GIMP's or `to_gpl`'s; none when the text is not one. Its name is empty when it has none.
[[nodiscard]] std::optional<Palette> palette_from_gpl(std::string_view text);

} // namespace olagarro::paint
