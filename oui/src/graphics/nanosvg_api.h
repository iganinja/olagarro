#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// NanoSVG behind a door of OUI's own, so its warnings stay in the one file that holds it.
namespace olagarro::oui::nanosvg_api
{

// `svg` drawn `width` by `height` pixels as bytes R, G, B, A with straight alpha, fitted and centred; none when it
// cannot be read.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> rasterize(std::string svg, int width, int height);

} // namespace olagarro::oui::nanosvg_api
