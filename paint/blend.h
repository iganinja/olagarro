#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace olagarro::paint
{

// How a canvas's pixels mix with those under it: Paint.NET's modes.
enum class BlendMode
{
    normal,
    multiply,
    additive,
    color_burn,
    color_dodge,
    reflect,
    glow,
    overlay,
    difference,
    negation,
    lighten,
    darken,
    screen,
    xor_,
};

// Every mode, in the order Paint.NET lists them, and their names.
[[nodiscard]] const std::vector<BlendMode>& all_blend_modes();
[[nodiscard]] std::string_view blend_mode_name(BlendMode mode);

// One channel of `above` over one of `below`, as `mode` mixes them, both from 0 to 1, before alpha is reckoned.
[[nodiscard]] inline double mix_channel(double below, double above, BlendMode mode)
{
    switch(mode)
    {
    case BlendMode::normal:
        return above;
    case BlendMode::multiply:
        return below * above;
    case BlendMode::additive:
        return std::min(1.0, below + above);
    case BlendMode::color_burn:
        return above <= 0.0 ? 0.0 : std::max(0.0, 1.0 - (1.0 - below) / above);
    case BlendMode::color_dodge:
        return above >= 1.0 ? 1.0 : std::min(1.0, below / (1.0 - above));
    case BlendMode::reflect:
        return above >= 1.0 ? 1.0 : std::min(1.0, below * below / (1.0 - above));
    case BlendMode::glow:
        return below >= 1.0 ? 1.0 : std::min(1.0, above * above / (1.0 - below));
    case BlendMode::overlay:
        return below < 0.5 ? 2.0 * below * above : 1.0 - 2.0 * (1.0 - below) * (1.0 - above);
    case BlendMode::difference:
        return std::abs(below - above);
    case BlendMode::negation:
        return 1.0 - std::abs(1.0 - below - above);
    case BlendMode::lighten:
        return std::max(below, above);
    case BlendMode::darken:
        return std::min(below, above);
    case BlendMode::screen:
        return 1.0 - (1.0 - below) * (1.0 - above);
    case BlendMode::xor_:
        break;
    }
    return above;
}

// `above` laid over `below` as `mode` mixes them, `above` seen through by `opacity`, from 0 to 1, as the W3C's
// compositing does it: where `below` is see-through, `above` shows as it is, and where it is solid, mixed. Colours are
// straight, not multiplied by their alpha, and so is the result.
template <typename PixelType>
[[nodiscard]] PixelType blend(const PixelType& below, const PixelType& above, double opacity, BlendMode mode)
{
    using Channel = typename PixelType::ChannelType;
    constexpr double full{PixelType::full};
    constexpr int colors{PixelType::channels - 1};
    const auto above_alpha{above.alpha() / full * opacity};
    const auto below_alpha{below.alpha() / full};
    const auto alpha{above_alpha + below_alpha * (1.0 - above_alpha)};
    PixelType result;
    if(alpha <= 0.0)
    {
        return result;
    }
    for(int index{}; index < colors; ++ index)
    {
        const auto backdrop{below.value[index] / full};
        const auto source{above.value[index] / full};
        // Xor works on the bits of the channels, as Paint.NET's does.
        const auto mixed{mode == BlendMode::xor_ ? static_cast<Channel>(below.value[index] ^ above.value[index]) / full
                                                 : mix_channel(backdrop, source, mode)};
        const auto shown{(1.0 - below_alpha) * source + below_alpha * mixed};
        const auto color{(shown * above_alpha + backdrop * below_alpha * (1.0 - above_alpha)) / alpha};
        result.value[index] = static_cast<Channel>(std::lround(std::clamp(color, 0.0, 1.0) * full));
    }
    result.value[colors] = static_cast<Channel>(std::lround(alpha * full));
    return result;
}

} // namespace olagarro::paint
