#include <paint/fill.h>

#include <paint/blend.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace olagarro::paint
{

namespace
{

// Each pixel's channels as numbers from 0 to 1, red, green, blue and alpha; grey repeated in the three colours.
[[nodiscard]] std::vector<std::array<float, 4>> unit_pixels(const Image& image)
{
    std::vector<std::array<float, 4>> result(static_cast<std::size_t>(image.width) * image.height);
    visit_format(image.format,
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     const auto full{static_cast<float>(PixelType::full)};
                     for(int y{}; y < image.height; ++ y)
                     {
                         for(int x{}; x < image.width; ++ x)
                         {
                             const auto pixel{image.pixel<PixelType>(x, y)};
                             auto& out{result[static_cast<std::size_t>(y) * image.width + x]};
                             if constexpr(PixelType::channels == 2)
                             {
                                 const auto gray{pixel.value[0] / full};
                                 out = {gray, gray, gray, pixel.value[1] / full};
                             }
                             else
                             {
                                 out = {pixel.value[0] / full, pixel.value[1] / full, pixel.value[2] / full,
                                        pixel.value[3] / full};
                             }
                         }
                     }
                 });
    return result;
}

} // namespace

std::vector<std::uint8_t> fill_region(const Image& source, int x, int y, const FillSettings& settings)
{
    const auto width{source.width};
    const auto height{source.height};
    std::vector<std::uint8_t> region(static_cast<std::size_t>(width) * height, 0);
    if(x < 0 || y < 0 || x >= width || y >= height)
    {
        return region;
    }
    const auto pixels{unit_pixels(source)};
    const auto seed{pixels[static_cast<std::size_t>(y) * width + x]};
    // The largest distance, between opposite corners of the four channels, is 2.
    const auto limit{static_cast<float>(std::clamp(settings.tolerance, 0.0, 1.0) * 2.0)};
    const auto similar{[&](std::size_t index)
                       {
                           const auto& pixel{pixels[index]};
                           auto sum{0.0f};
                           for(int channel{}; channel < 4; ++ channel)
                           {
                               const auto difference{pixel[channel] - seed[channel]};
                               sum += difference * difference;
                           }
                           // A small allowance so 0% still takes the very same colour after rounding.
                           return std::sqrt(sum) <= limit + 1e-6f;
                       }};
    if(settings.global)
    {
        for(std::size_t index{}; index < region.size(); ++ index)
        {
            region[index] = similar(index) ? 255 : 0;
        }
        return region;
    }
    // Rows filled span by span, with the spans above and below waiting their turn.
    std::vector<std::pair<int, int>> waiting{{x, y}};
    while(!waiting.empty())
    {
        const auto start{waiting.back().first};
        const auto row{waiting.back().second};
        waiting.pop_back();
        const auto at{[&](int column) { return static_cast<std::size_t>(row) * width + column; }};
        if(region[at(start)] != 0 || !similar(at(start)))
        {
            continue;
        }
        auto left{start};
        while(left > 0 && region[at(left - 1)] == 0 && similar(at(left - 1)))
        {
            -- left;
        }
        auto right{start};
        while(right + 1 < width && region[at(right + 1)] == 0 && similar(at(right + 1)))
        {
            ++ right;
        }
        for(auto column{left}; column <= right; ++ column)
        {
            region[at(column)] = 255;
        }
        for(const auto next_row : {row - 1, row + 1})
        {
            if(next_row < 0 || next_row >= height)
            {
                continue;
            }
            auto inside{false};
            for(auto column{left}; column <= right; ++ column)
            {
                const auto index{static_cast<std::size_t>(next_row) * width + column};
                const auto joins{region[index] == 0 && similar(index)};
                if(joins && !inside)
                {
                    waiting.emplace_back(column, next_row);
                }
                inside = joins;
            }
        }
    }
    return region;
}

std::vector<TileIndex> fill_reached(Canvas& canvas, const Selection& selection, const std::vector<std::uint8_t>& region,
                                    const Color& color, bool exact)
{
    const auto width{selection.width()};
    const auto height{selection.height()};
    Selection where{width, height};
    // The region inside the selection, as much as the selection selects.
    Image mask{PixelFormat::gray_alpha8, width, height};
    for(int row{}; row < height; ++ row)
    {
        for(int column{}; column < width; ++ column)
        {
            const auto value{region[static_cast<std::size_t>(row) * width + column]};
            if(value != 0)
            {
                const auto selected{selection.at(column, row)};
                mask.set_pixel(column, row, GrayAlpha8{{static_cast<std::uint8_t>(value * selected / 255), 255}});
            }
        }
    }
    Canvas mask_canvas{PixelFormat::gray_alpha8, width, height};
    mask_canvas.write(mask, 0, 0);
    where.set_mask(std::move(mask_canvas));
    return fill_selected(canvas, where, color, exact);
}

std::vector<TileIndex> bucket_fill(Canvas& canvas, const Selection& selection, int x, int y, const Color& color,
                                   const FillSettings& settings)
{
    const auto region{fill_region(canvas.read(0, 0, canvas.width(), canvas.height()), x, y, settings)};
    return fill_reached(canvas, selection, region, color, settings.exact);
}

} // namespace olagarro::paint
