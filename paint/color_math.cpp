#include <paint/color_math.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace olagarro::paint
{

Hsv to_hsv(const Color& color)
{
    const auto high{std::max({color.red, color.green, color.blue})};
    const auto low{std::min({color.red, color.green, color.blue})};
    const auto span{high - low};
    Hsv hsv{0.0, high > 0.0 ? span / high : 0.0, high};
    if(span <= 0.0)
    {
        return hsv;
    }
    if(high == color.red)
    {
        hsv.hue = 60.0 * std::fmod((color.green - color.blue) / span + 6.0, 6.0);
    }
    else if(high == color.green)
    {
        hsv.hue = 60.0 * ((color.blue - color.red) / span + 2.0);
    }
    else
    {
        hsv.hue = 60.0 * ((color.red - color.green) / span + 4.0);
    }
    return hsv;
}

Color from_hsv(const Hsv& hsv, double alpha)
{
    const auto hue{std::fmod(std::fmod(hsv.hue, 360.0) + 360.0, 360.0) / 60.0};
    const auto chroma{hsv.value * hsv.saturation};
    const auto second{chroma * (1.0 - std::abs(std::fmod(hue, 2.0) - 1.0))};
    const auto base{hsv.value - chroma};
    double red{};
    double green{};
    double blue{};
    switch(static_cast<int>(hue))
    {
    case 0:
        red = chroma, green = second;
        break;
    case 1:
        red = second, green = chroma;
        break;
    case 2:
        green = chroma, blue = second;
        break;
    case 3:
        green = second, blue = chroma;
        break;
    case 4:
        red = second, blue = chroma;
        break;
    default:
        red = chroma, blue = second;
        break;
    }
    return {red + base, green + base, blue + base, alpha};
}

} // namespace olagarro::paint
