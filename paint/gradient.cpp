#include <paint/gradient.h>

#include <paint/blend.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace olagarro::paint
{

const std::vector<GradientKind>& all_gradient_kinds()
{
    static const std::vector<GradientKind> kinds{GradientKind::linear, GradientKind::linear_reflected,
                                                 GradientKind::diamond, GradientKind::radial, GradientKind::conical};
    return kinds;
}

std::string_view gradient_name(GradientKind kind)
{
    switch(kind)
    {
    case GradientKind::linear:
        return "Linear";
    case GradientKind::linear_reflected:
        return "Linear Reflected";
    case GradientKind::diamond:
        return "Linear Diamond";
    case GradientKind::radial:
        return "Radial";
    case GradientKind::conical:
        break;
    }
    return "Conical";
}

double gradient_position(GradientKind kind, Position start, Position end, Position point)
{
    const auto dx{end.x - start.x};
    const auto dy{end.y - start.y};
    const auto length{std::sqrt(dx * dx + dy * dy)};
    if(length <= 0.0)
    {
        return 0.0;
    }
    const auto px{point.x - start.x};
    const auto py{point.y - start.y};
    // Along the line from start to end, and across it, in lengths of it.
    const auto along{(px * dx + py * dy) / (length * length)};
    const auto across{(px * -dy + py * dx) / (length * length)};
    switch(kind)
    {
    case GradientKind::linear:
        return std::clamp(along, 0.0, 1.0);
    case GradientKind::linear_reflected:
        return std::clamp(std::abs(along), 0.0, 1.0);
    case GradientKind::diamond:
        return std::clamp(std::abs(along) + std::abs(across), 0.0, 1.0);
    case GradientKind::radial:
        return std::clamp(std::sqrt(px * px + py * py) / length, 0.0, 1.0);
    case GradientKind::conical:
        break;
    }
    // Round the start, a whole turn from the line to the end and back to it.
    constexpr auto pi{3.14159265358979323846};
    auto angle{std::atan2(py, px) - std::atan2(dy, dx)};
    angle = std::fmod(angle + 2.0 * pi, 2.0 * pi);
    return 1.0 - std::abs(angle / pi - 1.0);
}

std::vector<TileIndex> draw_gradient(Canvas& canvas, const Canvas& base, const Selection& selection, GradientKind kind,
                                     Position start, Position end, const Color& from, const Color& to,
                                     bool transparency)
{
    const auto area{selection.bounds()};
    std::set<TileIndex> touched;
    visit_format(
        canvas.format(),
        [&](auto tag)
        {
            using PixelType = typename decltype(tag)::Type;
            using Channel = typename PixelType::ChannelType;
            const auto full{static_cast<double>(PixelType::full)};
            const auto channel{[full](double value)
                               { return static_cast<Channel>(std::lround(std::clamp(value, 0.0, 1.0) * full)); }};
            for(auto y{area.y}; y < area.y + area.height; ++ y)
            {
                for(auto x{area.x}; x < area.x + area.width; ++ x)
                {
                    const auto amount{selection.at(x, y) / 255.0};
                    if(amount <= 0.0)
                    {
                        continue;
                    }
                    const auto t{gradient_position(kind, start, end, {x + 0.5, y + 0.5})};
                    const TileIndex point{x / tile_size, y / tile_size};
                    const auto original{base.tile(point)};
                    const auto under{original != nullptr
                                         ? original->template pixel<PixelType>(x % tile_size, y % tile_size)
                                         : PixelType{}};
                    PixelType result;
                    if(transparency)
                    {
                        // The canvas's own pixel, as solid as the gradient says at its place.
                        result = under;
                        auto& alpha{result.value[PixelType::channels - 1]};
                        alpha = channel(alpha / full * (1.0 - t * amount));
                    }
                    else
                    {
                        const Color color{from.red + (to.red - from.red) * t, from.green + (to.green - from.green) * t,
                                          from.blue + (to.blue - from.blue) * t,
                                          from.alpha + (to.alpha - from.alpha) * t};
                        PixelType paint;
                        if constexpr(PixelType::channels == 2)
                        {
                            paint.value = {channel(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue),
                                           channel(color.alpha)};
                        }
                        else
                        {
                            paint.value = {channel(color.red), channel(color.green), channel(color.blue),
                                           channel(color.alpha)};
                        }
                        result = blend(under, paint, amount, BlendMode::normal);
                    }
                    canvas.writable_tile(point).set_pixel(x % tile_size, y % tile_size, result);
                    touched.insert(point);
                }
            }
        });
    return {touched.begin(), touched.end()};
}

std::vector<TileIndex> draw_dithered_gradient(Canvas& canvas, const Canvas& base, const Selection& selection,
                                              GradientKind kind, Position start, Position end, GrayAlpha8 from,
                                              GrayAlpha8 to)
{
    // Bayer's 4 by 4 order: a pixel takes `to` where the gradient is past its threshold.
    constexpr int bayer[4][4]{{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const auto area{selection.bounds()};
    std::set<TileIndex> touched;
    canvas = base;
    for(auto y{area.y}; y < area.y + area.height; ++ y)
    {
        for(auto x{area.x}; x < area.x + area.width; ++ x)
        {
            if(selection.at(x, y) < 128)
            {
                continue;
            }
            const auto t{gradient_position(kind, start, end, {x + 0.5, y + 0.5})};
            const auto threshold{(bayer[y & 3][x & 3] + 0.5) / 16.0};
            const TileIndex point{x / tile_size, y / tile_size};
            canvas.writable_tile(point).set_pixel(x % tile_size, y % tile_size, t >= threshold ? to : from);
            touched.insert(point);
        }
    }
    return {touched.begin(), touched.end()};
}

} // namespace olagarro::paint
