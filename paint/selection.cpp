#include <paint/selection.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace olagarro::paint
{

namespace
{

[[nodiscard]] std::uint8_t combined(std::uint8_t had, std::uint8_t shape, SelectionMode mode)
{
    switch(mode)
    {
    case SelectionMode::replace:
        return shape;
    case SelectionMode::add:
        return std::max(had, shape);
    case SelectionMode::subtract:
        return static_cast<std::uint8_t>((had * (255 - shape) + 127) / 255);
    case SelectionMode::intersect:
        return std::min(had, shape);
    case SelectionMode::invert:
        break;
    }
    return static_cast<std::uint8_t>(std::abs(had - shape));
}

} // namespace

Selection::Selection(int width, int height) : m_width{width}, m_height{height}
{
}

void Selection::empty_mask()
{
    m_mask.emplace(PixelFormat::gray_alpha8, m_width, m_height);
}

std::uint8_t Selection::at(int x, int y) const
{
    if(!m_mask)
    {
        return 255;
    }
    if(x < 0 || y < 0 || x >= m_width || y >= m_height)
    {
        return 0;
    }
    const auto tile{m_mask->tile({x / tile_size, y / tile_size})};
    return tile == nullptr ? 0 : tile->pixel<GrayAlpha8>(x % tile_size, y % tile_size).value[0];
}

void Selection::set(int x, int y, std::uint8_t value)
{
    m_mask->writable_tile({x / tile_size, y / tile_size})
        .set_pixel(x % tile_size, y % tile_size, GrayAlpha8{{value, 255}});
}

PixelArea Selection::bounds() const
{
    if(!m_mask)
    {
        return {0, 0, m_width, m_height};
    }
    auto left{m_width};
    auto top{m_height};
    auto right{-1};
    auto bottom{-1};
    for(const auto& [index, tile] : m_mask->tiles())
    {
        for(int y{}; y < tile_size; ++ y)
        {
            for(int x{}; x < tile_size; ++ x)
            {
                if(tile->pixel<GrayAlpha8>(x, y).value[0] == 0)
                {
                    continue;
                }
                const auto image_x{index.x * tile_size + x};
                const auto image_y{index.y * tile_size + y};
                left = std::min(left, image_x);
                top = std::min(top, image_y);
                right = std::max(right, image_x);
                bottom = std::max(bottom, image_y);
            }
        }
    }
    if(right < 0)
    {
        return {};
    }
    return {left, top, right - left + 1, bottom - top + 1};
}

void Selection::select_all()
{
    clear();
    add_rectangle({0, 0, m_width, m_height}, SelectionMode::replace);
}

void Selection::invert()
{
    if(!m_mask)
    {
        // Everything is selected, so nothing will be: an empty selection, not no selection.
        empty_mask();
        return;
    }
    combine({0, 0, m_width, m_height}, SelectionMode::invert, [](int, int) { return 1.0; });
}

template <typename Cover> void Selection::combine(const PixelArea& requested, SelectionMode mode, Cover cover)
{
    // Outside the shape, a replaced or intersected selection loses what it had: they start from nothing, and an
    // intersection reads what there was from the old mask.
    std::optional<Canvas> kept;
    if(!m_mask || mode == SelectionMode::replace || mode == SelectionMode::intersect)
    {
        if(m_mask && mode == SelectionMode::intersect)
        {
            kept = std::move(m_mask);
        }
        empty_mask();
    }
    const auto& read{kept ? *kept : *m_mask};
    const auto left{std::max(0, requested.x)};
    const auto top{std::max(0, requested.y)};
    const auto right{std::min(m_width, requested.x + requested.width)};
    const auto bottom{std::min(m_height, requested.y + requested.height)};
    if(left >= right || top >= bottom)
    {
        return;
    }
    // Tile by tile: each read once, and written only where something changes.
    for(auto tile_y{top / tile_size}; tile_y <= (bottom - 1) / tile_size; ++ tile_y)
    {
        for(auto tile_x{left / tile_size}; tile_x <= (right - 1) / tile_size; ++ tile_x)
        {
            const TileIndex index{tile_x, tile_y};
            const auto source{read.tile(index)};
            Tile* target{};
            const auto first_x{std::max(left, tile_x * tile_size)};
            const auto last_x{std::min(right, (tile_x + 1) * tile_size)};
            const auto first_y{std::max(top, tile_y * tile_size)};
            const auto last_y{std::min(bottom, (tile_y + 1) * tile_size)};
            for(auto y{first_y}; y < last_y; ++ y)
            {
                const auto local_y{y - tile_y * tile_size};
                const auto* const from{source != nullptr ? source->row(local_y) : nullptr};
                for(auto x{first_x}; x < last_x; ++ x)
                {
                    const auto local_x{x - tile_x * tile_size};
                    const std::uint8_t had{from != nullptr ? from[local_x * 2] : std::uint8_t{0}};
                    const auto shape{static_cast<std::uint8_t>(std::lround(std::clamp(cover(x, y), 0.0, 1.0) * 255.0))};
                    const auto value{mode == SelectionMode::intersect ? std::min(had, shape)
                                                                      : combined(had, shape, mode)};
                    // What a fresh mask holds is nothing; an old one holds `had`.
                    const auto holds{kept || mode == SelectionMode::replace ? std::uint8_t{0} : had};
                    if(value == holds)
                    {
                        continue;
                    }
                    if(target == nullptr)
                    {
                        target = &m_mask->writable_tile(index);
                    }
                    auto* const pixel{target->row(local_y) + local_x * 2};
                    pixel[0] = value;
                    pixel[1] = 255;
                }
            }
        }
    }
}

void Selection::add_rectangle(const PixelArea& area, SelectionMode mode)
{
    combine(area, mode, [](int, int) { return 1.0; });
}

void Selection::add_ellipse(double left, double top, double right, double bottom, SelectionMode mode)
{
    const auto center_x{(left + right) / 2.0};
    const auto center_y{(top + bottom) / 2.0};
    const auto radius_x{std::max(0.5, (right - left) / 2.0)};
    const auto radius_y{std::max(0.5, (bottom - top) / 2.0)};
    const PixelArea area{static_cast<int>(std::floor(left)), static_cast<int>(std::floor(top)),
                         static_cast<int>(std::ceil(right)) - static_cast<int>(std::floor(left)) + 1,
                         static_cast<int>(std::ceil(bottom)) - static_cast<int>(std::floor(top)) + 1};
    combine(area, mode,
            [&](int x, int y)
            {
                // Never past the rectangle it was drawn in.
                if(x + 0.5 < left || x + 0.5 > right || y + 0.5 < top || y + 0.5 > bottom)
                {
                    return 0.0;
                }
                // How far inside the edge the pixel's middle is, in pixels, by the ellipse's distance function over its
                // gradient: a pixel's worth of fade across the edge.
                const auto dx{(x + 0.5 - center_x) / radius_x};
                const auto dy{(y + 0.5 - center_y) / radius_y};
                const auto value{dx * dx + dy * dy - 1.0};
                const auto gradient{2.0 * std::sqrt(dx * dx / (radius_x * radius_x) + dy * dy / (radius_y * radius_y))};
                const auto distance{gradient > 0.0 ? value / gradient : value};
                return 0.5 - distance;
            });
}

void Selection::add_polygon(const std::vector<Position>& points, SelectionMode mode)
{
    if(points.size() < 3)
    {
        if(mode == SelectionMode::replace)
        {
            empty_mask();
        }
        return;
    }
    auto left{1e18};
    auto top{1e18};
    auto right{-1e18};
    auto bottom{-1e18};
    for(const auto& point : points)
    {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }
    const PixelArea area{static_cast<int>(std::floor(left)), static_cast<int>(std::floor(top)),
                         static_cast<int>(std::ceil(right)) - static_cast<int>(std::floor(left)) + 1,
                         static_cast<int>(std::ceil(bottom)) - static_cast<int>(std::floor(top)) + 1};
    // Each row's coverage worked out once: four lines through each row of pixels, and along each, the spans inside
    // with their ends' parts of a pixel counted.
    constexpr int lines{4};
    std::vector<double> row(static_cast<std::size_t>(area.width) + 1);
    std::vector<double> crossings;
    auto current_row{area.y - 1};
    const auto fill_row{[&](int y)
                        {
                            std::fill(row.begin(), row.end(), 0.0);
                            for(int line{}; line < lines; ++ line)
                            {
                                const auto scan_y{y + (line + 0.5) / lines};
                                crossings.clear();
                                for(std::size_t index{}; index < points.size(); ++ index)
                                {
                                    const auto& a{points[index]};
                                    const auto& b{points[(index + 1) % points.size()]};
                                    if((a.y <= scan_y) != (b.y <= scan_y))
                                    {
                                        crossings.push_back(a.x + (scan_y - a.y) / (b.y - a.y) * (b.x - a.x));
                                    }
                                }
                                std::sort(crossings.begin(), crossings.end());
                                for(std::size_t index{}; index + 1 < crossings.size(); index += 2)
                                {
                                    const auto from{crossings[index] - area.x};
                                    const auto to{crossings[index + 1] - area.x};
                                    for(auto x{std::max(0, static_cast<int>(std::floor(from)))};
                                        x < std::min(area.width, static_cast<int>(std::ceil(to))); ++ x)
                                    {
                                        const auto covered{std::min<double>(x + 1, to) - std::max<double>(x, from)};
                                        row[static_cast<std::size_t>(x)] += std::max(0.0, covered) / lines;
                                    }
                                }
                            }
                        }};
    combine(area, mode,
            [&](int x, int y)
            {
                if(y != current_row)
                {
                    fill_row(y);
                    current_row = y;
                }
                return row[static_cast<std::size_t>(x - area.x)];
            });
}

void Selection::add_coverage(const std::vector<std::uint8_t>& coverage, SelectionMode mode)
{
    combine({0, 0, m_width, m_height}, mode,
            [&](int x, int y) { return coverage[static_cast<std::size_t>(y) * m_width + x] / 255.0; });
}

void Selection::offset(int dx, int dy)
{
    if(!m_mask || (dx == 0 && dy == 0))
    {
        return;
    }
    const auto area{bounds()};
    const auto moved{m_mask->read(area.x, area.y, area.width, area.height)};
    empty_mask();
    // What moves outside the image is lost, as Paint.NET's selection stays inside it.
    const auto left{std::max(0, area.x + dx)};
    const auto top{std::max(0, area.y + dy)};
    const auto right{std::min(m_width, area.x + dx + area.width)};
    const auto bottom{std::min(m_height, area.y + dy + area.height)};
    for(auto y{top}; y < bottom; ++ y)
    {
        for(auto x{left}; x < right; ++ x)
        {
            const auto value{moved.pixel<GrayAlpha8>(x - area.x - dx, y - area.y - dy).value[0]};
            if(value != 0)
            {
                set(x, y, value);
            }
        }
    }
}

} // namespace olagarro::paint
