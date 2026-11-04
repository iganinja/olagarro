#pragma once

#include <paint/stroke.h>

#include <cstdint>
#include <vector>

namespace olagarro::paint
{

struct FillSettings
{
    // Every similar pixel of the image rather than those joined to the one clicked.
    bool global{false};
    // How different a pixel may be from the one clicked and still be filled, from 0 to 1, as the distance between
    // their channels, alpha among them.
    double tolerance{0.5};
    // Fills whole pixels only, as indices need.
    bool exact{false};
};

// Which pixels of `source` a fill from (x, y) reaches, as a coverage of 0 or 255 for each, row by row. `source` is
// what is compared: the canvas painted on, or a program's whole image as it shows.
[[nodiscard]] std::vector<std::uint8_t> fill_region(const Image& source, int x, int y, const FillSettings& settings);
// Fills what `region`, fill_region's, reaches with `color`, inside the selection; returns the tiles written. A program
// comparing its whole image fills with this.
std::vector<TileIndex> fill_reached(Canvas& canvas, const Selection& selection, const std::vector<std::uint8_t>& region,
                                    const Color& color, bool exact = false);
// Fills from (x, y) with `color` inside the selection, comparing the canvas's own pixels; returns the tiles written.
std::vector<TileIndex> bucket_fill(Canvas& canvas, const Selection& selection, int x, int y, const Color& color,
                                   const FillSettings& settings);

} // namespace olagarro::paint
