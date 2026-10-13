#include <oui/graphics/canvas.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace olagarro::oui
{

namespace
{

[[nodiscard]] std::uint32_t pack(Color color)
{
    return (std::uint32_t{color.red} << 16) | (std::uint32_t{color.green} << 8) | std::uint32_t{color.blue};
}

[[nodiscard]] Color unpack(std::uint32_t pixel)
{
    return {static_cast<std::uint8_t>(pixel >> 16), static_cast<std::uint8_t>(pixel >> 8),
            static_cast<std::uint8_t>(pixel)};
}

[[nodiscard]] std::uint8_t mix(std::uint8_t from, std::uint8_t to, int alpha)
{
    return static_cast<std::uint8_t>((from * (255 - alpha) + to * alpha + 127) / 255);
}

} // namespace

PixelRect intersect(const PixelRect& first, const PixelRect& second)
{
    const auto left{std::max(first.x, second.x)};
    const auto top{std::max(first.y, second.y)};
    const auto right{std::min(first.x + first.width, second.x + second.width)};
    const auto bottom{std::min(first.y + first.height, second.y + second.height)};
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

Canvas::Canvas(int width, int height) : m_width{0}, m_height{0}
{
    resize(width, height);
}
int Canvas::width() const
{
    return m_width;
}
int Canvas::height() const
{
    return m_height;
}
const std::vector<std::uint32_t>& Canvas::pixels() const
{
    return m_pixels;
}
Color Canvas::pixel_at(int x, int y) const
{
    if(x < 0 || y < 0 || x >= m_width || y >= m_height)
    {
        throw std::out_of_range{"The pixel is outside the canvas."};
    }
    return unpack(m_pixels[static_cast<std::size_t>(y * m_width + x)]);
}
void Canvas::resize(int width, int height)
{
    if(width < 0 || height < 0)
    {
        throw std::invalid_argument{"A canvas cannot have a negative size."};
    }
    m_width = width;
    m_height = height;
    m_pixels.assign(static_cast<std::size_t>(width * height), 0);
    m_clip = {0, 0, width, height};
}
void Canvas::set_clip(const PixelRect& clip)
{
    m_clip = intersect(clip, {0, 0, m_width, m_height});
}
const PixelRect& Canvas::clip() const
{
    return m_clip;
}
void Canvas::fill(const PixelRect& rect, Color color)
{
    const auto area{intersect(rect, m_clip)};
    const auto value{pack(color)};
    for(auto y{area.y}; y < area.y + area.height; ++ y)
    {
        auto* const row{&m_pixels[static_cast<std::size_t>(y * m_width)]};
        std::fill(row + area.x, row + area.x + area.width, value);
    }
}
void Canvas::copy(int x, int y, const std::uint32_t* pixels, int width, int height, int stride)
{
    const auto area{intersect({x, y, width, height}, m_clip)};
    for(auto row{area.y}; row < area.y + area.height; ++ row)
    {
        const auto* const source{pixels + static_cast<std::ptrdiff_t>(row - y) * stride + (area.x - x)};
        std::copy(source, source + area.width, &m_pixels[static_cast<std::size_t>(row * m_width + area.x)]);
    }
}
void Canvas::mix_over(int x, int y, const std::uint32_t* pixels, int width, int height, int stride)
{
    const auto area{intersect({x, y, width, height}, m_clip)};
    for(auto row{area.y}; row < area.y + area.height; ++ row)
    {
        const auto* const source{pixels + static_cast<std::ptrdiff_t>(row - y) * stride + (area.x - x)};
        for(auto column{0}; column < area.width; ++ column)
        {
            const auto pixel{source[column]};
            blend(area.x + column, row, unpack(pixel), static_cast<int>(pixel >> 24u));
        }
    }
}
void Canvas::blend(int x, int y, Color color, int alpha)
{
    if(alpha <= 0 || x < m_clip.x || y < m_clip.y || x >= m_clip.x + m_clip.width || y >= m_clip.y + m_clip.height)
    {
        return;
    }
    auto& pixel{m_pixels[static_cast<std::size_t>(y * m_width + x)]};
    if(alpha >= 255)
    {
        pixel = pack(color);
        return;
    }
    const auto under{unpack(pixel)};
    pixel = pack(
        {mix(under.red, color.red, alpha), mix(under.green, color.green, alpha), mix(under.blue, color.blue, alpha)});
}
void Canvas::fill_polygon(const std::vector<PixelPoint>& points, Color color)
{
    if(points.size() < 3)
    {
        return;
    }
    auto left{points.front().x};
    auto right{left};
    auto top{points.front().y};
    auto bottom{top};
    for(const auto& point : points)
    {
        left = std::min(left, point.x);
        right = std::max(right, point.x);
        top = std::min(top, point.y);
        bottom = std::max(bottom, point.y);
    }
    // 4x4 samples a pixel.
    constexpr int samples{4};
    for(auto y{static_cast<int>(std::floor(top))}; y <= static_cast<int>(std::ceil(bottom)); ++ y)
    {
        for(auto x{static_cast<int>(std::floor(left))}; x <= static_cast<int>(std::ceil(right)); ++ x)
        {
            int inside{};
            for(int sample{}; sample < samples * samples; ++ sample)
            {
                const auto sx{static_cast<float>(x) + (static_cast<float>(sample % samples) + 0.5f) / samples};
                const auto sy{static_cast<float>(y) + (static_cast<float>(sample / samples) + 0.5f) / samples};
                bool in{false};
                for(std::size_t i{}, j{points.size() - 1}; i < points.size(); j = i++)
                {
                    const auto& a{points[i]};
                    const auto& b{points[j]};
                    if((a.y > sy) != (b.y > sy) && sx < (b.x - a.x) * (sy - a.y) / (b.y - a.y) + a.x)
                    {
                        in = !in;
                    }
                }
                inside += in ? 1 : 0;
            }
            blend(x, y, color, inside * 255 / (samples * samples));
        }
    }
}
void Canvas::draw_line(PixelPoint from, PixelPoint to, float width, Color color)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{std::sqrt(dx * dx + dy * dy)};
    if(length <= 0.0f)
    {
        return;
    }
    const auto nx{-dy / length * width / 2};
    const auto ny{dx / length * width / 2};
    fill_polygon(
        {{from.x + nx, from.y + ny}, {to.x + nx, to.y + ny}, {to.x - nx, to.y - ny}, {from.x - nx, from.y - ny}},
        color);
}
void Canvas::fill_blended(const PixelRect& rect, Color color, int alpha)
{
    const auto area{intersect(rect, m_clip)};
    for(auto y{area.y}; y < area.y + area.height; ++ y)
    {
        for(auto x{area.x}; x < area.x + area.width; ++ x)
        {
            blend(x, y, color, alpha);
        }
    }
}
void Canvas::draw_glyph(const GlyphBitmap& glyph, int pen_x, int baseline, Color color, int alpha)
{
    for(int row{}; row < glyph.height; ++ row)
    {
        for(int column{}; column < glyph.width; ++ column)
        {
            blend(pen_x + glyph.left + column, baseline + glyph.top + row, color,
                  glyph.coverage[static_cast<std::size_t>(row * glyph.width + column)] * alpha / 255);
        }
    }
}

} // namespace olagarro::oui
