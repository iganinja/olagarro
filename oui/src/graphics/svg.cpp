#include <oui/graphics/svg.h>

#include "nanosvg_api.h"

#include <string>

namespace olagarro::oui
{

std::optional<Picture> draw_svg(std::string_view svg, int width, int height)
{
    const auto bytes{nanosvg_api::rasterize(std::string{svg}, width, height)};
    if(!bytes)
    {
        return std::nullopt;
    }
    Picture picture{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height)};
    for(std::size_t index{}; index < picture.pixels.size(); ++ index)
    {
        const auto* const pixel{&(*bytes)[index * 4u]};
        picture.pixels[index] = static_cast<std::uint32_t>(pixel[3]) << 24u |
                                static_cast<std::uint32_t>(pixel[0]) << 16u |
                                static_cast<std::uint32_t>(pixel[1]) << 8u | static_cast<std::uint32_t>(pixel[2]);
    }
    return picture;
}

} // namespace olagarro::oui
