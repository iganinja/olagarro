#pragma once

#include <cstdint>

namespace olagarro::oui
{

struct Color
{
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};

    [[nodiscard]] constexpr bool operator==(const Color& other) const
    {
        return red == other.red && green == other.green && blue == other.blue;
    }

    [[nodiscard]] constexpr bool operator!=(const Color& other) const { return !(*this == other); }
};

} // namespace olagarro::oui
