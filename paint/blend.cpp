#include <paint/blend.h>

namespace olagarro::paint
{

const std::vector<BlendMode>& all_blend_modes()
{
    static const std::vector<BlendMode> modes{
        BlendMode::normal,  BlendMode::multiply, BlendMode::additive, BlendMode::color_burn, BlendMode::color_dodge,
        BlendMode::reflect, BlendMode::glow,     BlendMode::overlay,  BlendMode::difference, BlendMode::negation,
        BlendMode::lighten, BlendMode::darken,   BlendMode::screen,   BlendMode::xor_};
    return modes;
}

std::string_view blend_mode_name(BlendMode mode)
{
    switch(mode)
    {
    case BlendMode::normal:
        return "Normal";
    case BlendMode::multiply:
        return "Multiply";
    case BlendMode::additive:
        return "Additive";
    case BlendMode::color_burn:
        return "Color Burn";
    case BlendMode::color_dodge:
        return "Color Dodge";
    case BlendMode::reflect:
        return "Reflect";
    case BlendMode::glow:
        return "Glow";
    case BlendMode::overlay:
        return "Overlay";
    case BlendMode::difference:
        return "Difference";
    case BlendMode::negation:
        return "Negation";
    case BlendMode::lighten:
        return "Lighten";
    case BlendMode::darken:
        return "Darken";
    case BlendMode::screen:
        return "Screen";
    case BlendMode::xor_:
        break;
    }
    return "Xor";
}

} // namespace olagarro::paint
