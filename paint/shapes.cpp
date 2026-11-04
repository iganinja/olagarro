#include <paint/shapes.h>

#include <paint/blend.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <set>

namespace olagarro::paint
{

namespace
{

// How much of a pixel whose middle is `distance` from an edge is inside, with a pixel's worth of fade across the edge,
// or none of it when not antialiased.
[[nodiscard]] double inside(double distance, bool antialias)
{
    if(!antialias)
    {
        return distance <= 0.0 ? 1.0 : 0.0;
    }
    return std::clamp(0.5 - distance, 0.0, 1.0);
}

// Distances from a point to the edge of a shape, negative inside.
[[nodiscard]] double rectangle_distance(double x, double y, double center_x, double center_y, double half_width,
                                        double half_height, double radius)
{
    const auto qx{std::abs(x - center_x) - half_width + radius};
    const auto qy{std::abs(y - center_y) - half_height + radius};
    const auto outside{std::hypot(std::max(qx, 0.0), std::max(qy, 0.0))};
    return outside + std::min(std::max(qx, qy), 0.0) - radius;
}

[[nodiscard]] double ellipse_distance(double x, double y, double center_x, double center_y, double radius_x,
                                      double radius_y)
{
    const auto dx{(x - center_x) / radius_x};
    const auto dy{(y - center_y) / radius_y};
    const auto value{dx * dx + dy * dy - 1.0};
    const auto gradient{2.0 * std::sqrt(dx * dx / (radius_x * radius_x) + dy * dy / (radius_y * radius_y))};
    return gradient > 0.0 ? value / gradient : -std::min(radius_x, radius_y);
}

[[nodiscard]] double segment_distance(double x, double y, Position from, Position to)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{dx * dx + dy * dy};
    const auto along{length > 0.0 ? std::clamp(((x - from.x) * dx + (y - from.y) * dy) / length, 0.0, 1.0) : 0.0};
    return std::hypot(from.x + along * dx - x, from.y + along * dy - y);
}

// Distance to a triangle, negative inside.
[[nodiscard]] double triangle_distance(double x, double y, const std::array<Position, 3>& corners)
{
    auto nearest{1e18};
    auto sign_changes{0};
    for(std::size_t index{}; index < 3; ++ index)
    {
        const auto& a{corners[index]};
        const auto& b{corners[(index + 1) % 3]};
        nearest = std::min(nearest, segment_distance(x, y, a, b));
        const auto cross{(b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x)};
        sign_changes += cross >= 0.0 ? 1 : 0;
    }
    const auto in{sign_changes == 0 || sign_changes == 3};
    return in ? -nearest : nearest;
}

// Mixes `color` over the canvas at each pixel of the box from `left`, `top` to `right`, `bottom` by `cover(x, y)`,
// inside the selection.
template <typename Cover>
void paint(Canvas& canvas, const Selection& selection, double left, double top, double right, double bottom,
           const Color& color, bool antialias, Cover cover, std::set<TileIndex>& touched)
{
    const auto first_x{std::max(0, static_cast<int>(std::floor(left)) - 1)};
    const auto first_y{std::max(0, static_cast<int>(std::floor(top)) - 1)};
    const auto last_x{std::min(selection.width() - 1, static_cast<int>(std::ceil(right)) + 1)};
    const auto last_y{std::min(selection.height() - 1, static_cast<int>(std::ceil(bottom)) + 1)};
    visit_format(canvas.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     using Channel = typename PixelType::ChannelType;
                     const auto channel{[](double value) {
                         return static_cast<Channel>(std::lround(std::clamp(value, 0.0, 1.0) * PixelType::full));
                     }};
                     PixelType ink;
                     if constexpr(PixelType::channels == 2)
                     {
                         ink.value = {channel(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue),
                                      channel(color.alpha)};
                     }
                     else
                     {
                         ink.value = {channel(color.red), channel(color.green), channel(color.blue),
                                      channel(color.alpha)};
                     }
                     // An opaque ink fully over a pixel replaces it, without mixing.
                     const auto opaque{ink.value.back() == PixelType::full};
                     const auto& mask{selection.mask()};
                     // A tile at a time: its pixels and the selection's are looked up once, and it is only copied to
                     // be written when something lands on it.
                     for(auto tile_y{tile_of(first_y)}; tile_y <= tile_of(last_y); ++ tile_y)
                     {
                         for(auto tile_x{tile_of(first_x)}; tile_x <= tile_of(last_x); ++ tile_x)
                         {
                             const TileIndex point{tile_x, tile_y};
                             const auto selected{mask ? mask->tile(point) : nullptr};
                             if(mask && selected == nullptr)
                             {
                                 continue;
                             }
                             Tile* tile{};
                             const auto from_y{std::max(first_y, tile_y * tile_size)};
                             const auto to_y{std::min(last_y, tile_y * tile_size + tile_size - 1)};
                             const auto from_x{std::max(first_x, tile_x * tile_size)};
                             const auto to_x{std::min(last_x, tile_x * tile_size + tile_size - 1)};
                             for(auto y{from_y}; y <= to_y; ++ y)
                             {
                                 const auto local_y{y - tile_y * tile_size};
                                 const auto* selected_row{selected != nullptr ? selected->row(local_y) : nullptr};
                                 for(auto x{from_x}; x <= to_x; ++ x)
                                 {
                                     const auto local_x{x - tile_x * tile_size};
                                     const auto how_selected{selected_row != nullptr ? selected_row[local_x * 2] : 255};
                                     if(how_selected == 0)
                                     {
                                         continue;
                                     }
                                     auto amount{cover(x + 0.5, y + 0.5) * how_selected / 255.0};
                                     // Without antialiasing nothing is mixed, the selection's soft edge neither.
                                     if(!antialias)
                                     {
                                         amount = amount >= 0.5 ? 1.0 : 0.0;
                                     }
                                     if(amount <= 0.0)
                                     {
                                         continue;
                                     }
                                     if(tile == nullptr)
                                     {
                                         tile = &canvas.writable_tile(point);
                                         touched.insert(point);
                                     }
                                     tile->set_pixel(local_x, local_y,
                                                     opaque && amount >= 1.0
                                                         ? ink
                                                         : blend(tile->template pixel<PixelType>(local_x, local_y), ink,
                                                                 amount, BlendMode::normal));
                                 }
                             }
                         }
                     }
                 });
}

} // namespace

std::string_view shape_name(ShapeKind kind)
{
    switch(kind)
    {
    case ShapeKind::rectangle:
        return "Rectangle";
    case ShapeKind::rounded_rectangle:
        return "Rounded Rectangle";
    case ShapeKind::ellipse:
        break;
    }
    return "Ellipse";
}

std::string_view shape_style_name(ShapeStyle style)
{
    switch(style)
    {
    case ShapeStyle::outline:
        return "Draw Shape Outline";
    case ShapeStyle::filled:
        return "Draw Filled Shape";
    case ShapeStyle::outline_and_fill:
        break;
    }
    return "Draw Filled Shape With Outline";
}

std::vector<TileIndex> draw_shape(Canvas& canvas, const Selection& selection, ShapeKind kind, ShapeStyle style,
                                  Position corner, Position opposite, const Color& outline, const Color& fill,
                                  double width, bool antialias)
{
    const auto left{std::min(corner.x, opposite.x)};
    const auto top{std::min(corner.y, opposite.y)};
    const auto right{std::max(corner.x, opposite.x)};
    const auto bottom{std::max(corner.y, opposite.y)};
    const auto center_x{(left + right) / 2.0};
    const auto center_y{(top + bottom) / 2.0};
    const auto half_width{(right - left) / 2.0};
    const auto half_height{(bottom - top) / 2.0};
    const auto radius{kind == ShapeKind::rounded_rectangle ? std::min({half_width, half_height, 20.0}) : 0.0};
    const auto distance{[&](double x, double y)
                        {
                            if(kind == ShapeKind::ellipse)
                            {
                                return ellipse_distance(x, y, center_x, center_y, std::max(0.5, half_width),
                                                        std::max(0.5, half_height));
                            }
                            return rectangle_distance(x, y, center_x, center_y, half_width, half_height, radius);
                        }};
    std::set<TileIndex> touched;
    if(style != ShapeStyle::outline)
    {
        const auto& inner{style == ShapeStyle::filled ? outline : fill};
        // Filled under the outline, as far as the outline's middle, so the two meet without a gap.
        const auto inset{style == ShapeStyle::outline_and_fill ? width / 2.0 : 0.0};
        paint(
            canvas, selection, left, top, right, bottom, inner, antialias,
            [&](double x, double y) { return inside(distance(x, y) + inset, antialias); }, touched);
    }
    if(style != ShapeStyle::filled)
    {
        // The outline inside the edge, `width` deep.
        paint(
            canvas, selection, left, top, right, bottom, outline, antialias,
            [&](double x, double y)
            {
                const auto from_edge{distance(x, y)};
                return std::min(inside(from_edge, antialias), inside(-from_edge - width, antialias));
            },
            touched);
    }
    return {touched.begin(), touched.end()};
}

std::vector<Position> curve_through(const std::array<Position, 4>& through, int per_span)
{
    // Catmull-Rom through the four, with a point mirrored past each end so the curve leaves them heading for the next.
    const std::array<Position, 6> points{
        Position{2.0 * through[0].x - through[1].x, 2.0 * through[0].y - through[1].y},
        through[0],
        through[1],
        through[2],
        through[3],
        Position{2.0 * through[3].x - through[2].x, 2.0 * through[3].y - through[2].y}};
    std::vector<Position> along;
    for(std::size_t span{1}; span <= 3; ++ span)
    {
        const auto& p0{points[span - 1]};
        const auto& p1{points[span]};
        const auto& p2{points[span + 1]};
        const auto& p3{points[span + 2]};
        for(int step{}; step < per_span || (span == 3 && step == per_span); ++ step)
        {
            const auto t{static_cast<double>(step) / per_span};
            const auto t2{t * t};
            const auto t3{t2 * t};
            const auto blend{[&](double a, double b, double c, double d) {
                return 0.5 * (2.0 * b + (c - a) * t + (2.0 * a - 5.0 * b + 4.0 * c - d) * t2 +
                              (3.0 * b - a - 3.0 * c + d) * t3);
            }};
            along.push_back({blend(p0.x, p1.x, p2.x, p3.x), blend(p0.y, p1.y, p2.y, p3.y)});
        }
    }
    return along;
}

std::vector<TileIndex> draw_arc(Canvas& canvas, const Selection& selection, Position center, double radius,
                                double start, double sweep, const Color& color, double width, bool antialias)
{
    constexpr auto turn{2.0 * 3.14159265358979323846};
    const auto whole{std::abs(sweep) >= turn - 1e-9};
    const auto end{start + sweep};
    const Position first{center.x + std::cos(start) * radius, center.y + std::sin(start) * radius};
    const Position last{center.x + std::cos(end) * radius, center.y + std::sin(end) * radius};
    // Whether a direction from the centre lies within the arc: how far round from its start, the way it turns.
    const auto within{[&](double angle)
                      {
                          if(whole)
                          {
                              return true;
                          }
                          const auto from_start{sweep >= 0.0 ? angle - start : start - angle};
                          const auto round{from_start - std::floor(from_start / turn) * turn};
                          return round <= std::abs(sweep);
                      }};
    const auto reach{radius + width};
    std::set<TileIndex> touched;
    paint(
        canvas, selection, center.x - reach, center.y - reach, center.x + reach, center.y + reach, color, antialias,
        [&, near{std::max(0.0, radius - width / 2.0 - 1.0)}, far{radius + width / 2.0 + 1.0}](double x, double y)
        {
            const auto dx{x - center.x};
            const auto dy{y - center.y};
            // Off the band round the circle that the arc and its round ends lie in: nothing, without the sums below,
            // which most pixels of the box are.
            const auto squared{dx * dx + dy * dy};
            if(squared < near * near || squared > far * far)
            {
                return 0.0;
            }
            // To the circle where the pixel's direction is within the arc; to the nearer end, rounded, where not.
            const auto distance{within(std::atan2(dy, dx)) ? std::abs(std::hypot(dx, dy) - radius)
                                                           : std::min(std::hypot(x - first.x, y - first.y),
                                                                      std::hypot(x - last.x, y - last.y))};
            return inside(distance - width / 2.0, antialias);
        },
        touched);
    return {touched.begin(), touched.end()};
}

std::vector<TileIndex> draw_curve(Canvas& canvas, const Selection& selection, const std::array<Position, 4>& through,
                                  const Color& color, double width, bool antialias, const LineSettings& settings)
{
    auto along{curve_through(through, 24)};
    const auto head{std::max(8.0, width * 3.0)};
    // An end's direction: from the point a head's length back along the curve, so a bend near it does not tip it.
    const auto direction{[&](bool at_end)
                         {
                             const auto& tip{at_end ? along.back() : along.front()};
                             Position back{tip};
                             for(std::size_t index{1}; index < along.size(); ++ index)
                             {
                                 const auto& point{at_end ? along[along.size() - 1 - index] : along[index]};
                                 back = point;
                                 if(std::hypot(point.x - tip.x, point.y - tip.y) >= head)
                                 {
                                     break;
                                 }
                             }
                             const auto length{std::hypot(tip.x - back.x, tip.y - back.y)};
                             return length > 0.0 ? Position{(tip.x - back.x) / length, (tip.y - back.y) / length}
                                                 : Position{at_end ? 1.0 : -1.0, 0.0};
                         }};
    const auto arrow{[&](Position tip, Position heading) -> std::array<Position, 3>
                     {
                         const Position back{tip.x - heading.x * head, tip.y - heading.y * head};
                         return {tip, Position{back.x - heading.y * head / 2.0, back.y + heading.x * head / 2.0},
                                 Position{back.x + heading.y * head / 2.0, back.y - heading.x * head / 2.0}};
                     }};
    const auto start_head{arrow(along.front(), direction(false))};
    const auto end_head{arrow(along.back(), direction(true))};
    // The line stops where an arrowhead begins.
    const auto trim{[&](bool at_end)
                    {
                        const auto tip{at_end ? along.back() : along.front()};
                        while(along.size() > 2)
                        {
                            const auto& next{at_end ? along[along.size() - 2] : along[1]};
                            if(std::hypot(next.x - tip.x, next.y - tip.y) >= head * 0.8)
                            {
                                break;
                            }
                            along.erase(at_end ? along.end() - 1 : along.begin());
                        }
                    }};
    if(settings.arrow_at_start)
    {
        trim(false);
    }
    if(settings.arrow_at_end)
    {
        trim(true);
    }
    const auto reach{std::max(width, head)};
    auto left{along.front().x};
    auto top{along.front().y};
    auto right{left};
    auto bottom{top};
    for(const auto& point : along)
    {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }
    for(const auto& tip : {start_head, end_head})
    {
        for(const auto& corner : tip)
        {
            left = std::min(left, corner.x);
            top = std::min(top, corner.y);
            right = std::max(right, corner.x);
            bottom = std::max(bottom, corner.y);
        }
    }
    std::set<TileIndex> touched;
    paint(
        canvas, selection, left - reach, top - reach, right + reach, bottom + reach, color, antialias,
        [&](double x, double y)
        {
            auto distance{1e18};
            for(std::size_t index{1}; index < along.size(); ++ index)
            {
                const auto& a{along[index - 1]};
                const auto& b{along[index]};
                // Segments whose box, grown by the width, leaves the pixel out cannot be the nearest that counts.
                const auto grown{width + 1.0};
                if(x < std::min(a.x, b.x) - grown || x > std::max(a.x, b.x) + grown || y < std::min(a.y, b.y) - grown ||
                   y > std::max(a.y, b.y) + grown)
                {
                    continue;
                }
                distance = std::min(distance, segment_distance(x, y, a, b));
            }
            distance -= width / 2.0;
            if(settings.arrow_at_start)
            {
                distance = std::min(distance, triangle_distance(x, y, start_head));
            }
            if(settings.arrow_at_end)
            {
                distance = std::min(distance, triangle_distance(x, y, end_head));
            }
            return inside(distance, antialias);
        },
        touched);
    return {touched.begin(), touched.end()};
}

std::vector<TileIndex> draw_line(Canvas& canvas, const Selection& selection, Position from, Position to,
                                 const Color& color, double width, bool antialias, const LineSettings& settings)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{std::hypot(dx, dy)};
    // Arrowheads three widths long and wide, at least eight pixels.
    const auto head{std::max(8.0, width * 3.0)};
    const auto ux{length > 0.0 ? dx / length : 1.0};
    const auto uy{length > 0.0 ? dy / length : 0.0};
    const auto arrow{[&](Position tip, double direction) -> std::array<Position, 3>
                     {
                         const Position back{tip.x - ux * head * direction, tip.y - uy * head * direction};
                         return {tip, Position{back.x - uy * head / 2.0, back.y + ux * head / 2.0},
                                 Position{back.x + uy * head / 2.0, back.y - ux * head / 2.0}};
                     }};
    // The line stops where an arrowhead begins.
    auto start{from};
    auto end{to};
    if(settings.arrow_at_start)
    {
        start = {from.x + ux * head * 0.8, from.y + uy * head * 0.8};
    }
    if(settings.arrow_at_end)
    {
        end = {to.x - ux * head * 0.8, to.y - uy * head * 0.8};
    }
    const auto start_head{arrow(from, -1.0)};
    const auto end_head{arrow(to, 1.0)};
    const auto reach{std::max(width, head)};
    std::set<TileIndex> touched;
    paint(
        canvas, selection, std::min(from.x, to.x) - reach, std::min(from.y, to.y) - reach,
        std::max(from.x, to.x) + reach, std::max(from.y, to.y) + reach, color, antialias,
        [&](double x, double y)
        {
            auto distance{segment_distance(x, y, start, end) - width / 2.0};
            if(settings.arrow_at_start)
            {
                distance = std::min(distance, triangle_distance(x, y, start_head));
            }
            if(settings.arrow_at_end)
            {
                distance = std::min(distance, triangle_distance(x, y, end_head));
            }
            return inside(distance, antialias);
        },
        touched);
    return {touched.begin(), touched.end()};
}

} // namespace olagarro::paint
