#pragma once

#include <paint/color.h>

namespace olagarro::paint
{

// Hue from 0 to 360, saturation and value from 0 to 1.
struct Hsv
{
    double hue{0.0};
    double saturation{0.0};
    double value{0.0};
};

[[nodiscard]] Hsv to_hsv(const Color& color);
// The colour of `hsv`, with `alpha`.
[[nodiscard]] Color from_hsv(const Hsv& hsv, double alpha = 1.0);

} // namespace olagarro::paint
