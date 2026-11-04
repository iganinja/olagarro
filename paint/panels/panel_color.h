#pragma once

#include <oui/color.h>
#include <oui/theme.h>

#include <algorithm>
#include <cstdint>

namespace olagarro::paint
{

// The colour of the panels at the sides, the tools and the colours: a little darker than the window's, or lighter in a
// dark theme, so they stand apart from it.
[[nodiscard]] inline oui::Color side_panel_color(const oui::Theme& theme)
{
    const auto& face{theme.status_background};
    const auto light{face.red * 299 + face.green * 587 + face.blue * 114 > 128 * 1000};
    const auto shade{[light](std::uint8_t channel)
                     {
                         return static_cast<std::uint8_t>(light ? channel * 88 / 100
                                                                : std::min(255, channel + (255 - channel) * 12 / 100));
                     }};
    return {shade(face.red), shade(face.green), shade(face.blue)};
}

} // namespace olagarro::paint
