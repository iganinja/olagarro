#include <oui/graphics/icon.h>

#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>

#include <algorithm>
#include <cmath>

namespace olagarro::oui
{

WindowIcon text_icon(std::string_view letters, Color background, Color foreground, int size)
{
    Canvas canvas{size, size};
    canvas.fill({0, 0, size, size}, background);
    // The largest cell font whose letters fit across with a margin either side.
    for(const auto height : {64, 48, 32, 24, 16})
    {
        auto font{built_in_cell_font(height)};
        const auto width{font->measure(letters)};
        // Each letter twice, a little apart, to be thick enough still when the desktop shows the icon small.
        const auto thicken{std::max(1, size / 48)};
        if(width + thicken <= size * 7 / 8)
        {
            const auto left{(size - width - thicken) / 2};
            const auto top{(size - font->line_height()) / 2};
            for(int shift{}; shift <= thicken; ++ shift)
            {
                font->draw(canvas, left + shift, top, letters, foreground);
            }
            break;
        }
    }
    // Corners rounded with a soft edge: each corner pixel as covered as it is by the circle that rounds it.
    const auto radius{static_cast<float>(size) / 6.0f};
    WindowIcon icon{size, {}};
    icon.pixels.reserve(canvas.pixels().size());
    for(int y{}; y < size; ++ y)
    {
        for(int x{}; x < size; ++ x)
        {
            const auto centre{[radius, size](int at)
                              {
                                  const auto middle{static_cast<float>(at) + 0.5f};
                                  return std::clamp(middle, radius, static_cast<float>(size) - radius);
                              }};
            const auto distance{std::hypot(static_cast<float>(x) + 0.5f - centre(x),
                                           static_cast<float>(y) + 0.5f - centre(y))};
            const auto coverage{std::clamp(radius + 0.5f - distance, 0.0f, 1.0f)};
            const auto alpha{static_cast<std::uint32_t>(std::lround(coverage * 255.0f))};
            icon.pixels.push_back((alpha << 24u) | (canvas.pixels()[static_cast<std::size_t>(y * size + x)] & 0xFFFFFFu));
        }
    }
    return icon;
}

} // namespace olagarro::oui
