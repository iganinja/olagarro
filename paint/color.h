#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace olagarro::paint
{

// A colour as four numbers from 0 to 1, red, green, blue and alpha, whatever the depth it is written at.
struct Color
{
    double red{0.0};
    double green{0.0};
    double blue{0.0};
    double alpha{1.0};

    [[nodiscard]] bool operator==(const Color& other) const
    {
        return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
    }
    [[nodiscard]] bool operator!=(const Color& other) const { return !(*this == other); }
};

// "#rrggbb", or "#rrggbbaa" when not solid.
[[nodiscard]] std::string to_hex(const Color& color);
[[nodiscard]] std::optional<Color> from_hex(std::string_view text);

} // namespace olagarro::paint
