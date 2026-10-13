#include "nanosvg_api.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>

namespace olagarro::oui::nanosvg_api
{

std::optional<std::vector<std::uint8_t>> rasterize(std::string svg, int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        return std::nullopt;
    }
    auto* const image{nsvgParse(svg.data(), "px", 96.0f)};
    if(image == nullptr)
    {
        return std::nullopt;
    }
    if(image->width <= 0.0f || image->height <= 0.0f || image->shapes == nullptr)
    {
        nsvgDelete(image);
        return std::nullopt;
    }
    auto* const rasterizer{nsvgCreateRasterizer()};
    const auto scale{std::min(static_cast<float>(width) / image->width, static_cast<float>(height) / image->height)};
    const auto left{(static_cast<float>(width) - image->width * scale) / 2.0f};
    const auto top{(static_cast<float>(height) - image->height * scale) / 2.0f};
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    nsvgRasterize(rasterizer, image, left, top, scale, pixels.data(), width, height, width * 4);
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);
    return pixels;
}

} // namespace olagarro::oui::nanosvg_api
