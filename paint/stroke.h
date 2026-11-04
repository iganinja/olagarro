#pragma once

#include <paint/canvas.h>
#include <paint/color.h>
#include <paint/geometry.h>
#include <paint/selection.h>

#include <map>
#include <optional>
#include <vector>

namespace olagarro::paint
{

struct BrushSettings
{
    // Across, in pixels.
    double width{8.0};
    // How much of the radius is solid before it fades to the edge: 1 is a hard brush, 0 fades from the middle.
    double hardness{0.75};
    bool antialias{true};
    // Every pixel either painted whole or left, the selection's soft edge too, as indices need: no mixing.
    bool exact{false};
};

enum class StrokeMode
{
    paint,
    erase,
    // Copies the canvas from `source_offset` away, as the Clone Stamp does.
    clone,
    // Paints over the pixels near enough to `match` only, as Recolor does.
    recolor,
};

// What a clone or a recolor stroke needs besides a colour.
struct StrokeSource
{
    // Where the pixels a clone copies are, from where it paints.
    int dx{0};
    int dy{0};
    // The colour Recolor replaces, and how far from it a pixel may be, from 0 to 1.
    Color match;
    double tolerance{0.5};
};

// A stroke of a brush being drawn on a canvas, from the pointer's first position to its last. Each pixel takes the most
// the stroke ever covered it, mixed over the pixel as it was before the stroke: where the stroke crosses itself a
// see-through colour does not build up.
class Stroke
{
  public:
    // Paints only where `selection` selects, as much as it selects.
    Stroke(Canvas& canvas, const Selection& selection, Color color, BrushSettings settings, StrokeMode mode,
           StrokeSource source = {});

    // Draws from the last point to `point`, in the image's pixels, whose centres are at half pixels. `pressure`, from 0
    // to 1, scales the brush's width at `point`, from what it was at the last point.
    void line_to(Position point, double pressure = 1.0);
    // The tiles written since last asked.
    [[nodiscard]] std::vector<TileIndex> take_changed();
    // The canvas as it was when the stroke began.
    [[nodiscard]] const Canvas& original() const { return m_original; }

  private:
    void cover_segment(Position from, Position to, double from_width, double to_width);
    // Mixes the stroke into the pixels of the tile at `point` inside `area`.
    void apply(TileIndex point, const PixelArea& area);
    [[nodiscard]] double coverage(double distance, double width) const;

    Canvas& m_canvas;
    const Selection& m_selection;
    // The canvas as it was when the stroke began, what every pixel is mixed over.
    Canvas m_original;
    int m_width;
    int m_height;
    Color m_color;
    BrushSettings m_settings;
    StrokeMode m_mode;
    StrokeSource m_source;
    std::optional<Position> m_last;
    double m_last_pressure{1.0};
    std::map<TileIndex, std::vector<float>> m_coverage;
    std::vector<TileIndex> m_changed;
};

// Mixes `color` over the canvas wherever `selection` selects, as much as it selects, as Backspace does; returns the
// tiles written.
// With `exact`, a pixel is filled whole where it is selected half or more, and left otherwise, as indices need.
std::vector<TileIndex> fill_selected(Canvas& canvas, const Selection& selection, const Color& color,
                                     bool exact = false);
// Takes away the canvas's pixels wherever `selection` selects, as much as it selects, as Delete does.
std::vector<TileIndex> erase_selected(Canvas& canvas, const Selection& selection, bool exact = false);

// The canvas's pixels inside the selection's bounds, each as see-through as the selection leaves it, and where they
// were: what Move Selected Pixels lifts.
struct Lifted
{
    Image pixels;
    int x{0};
    int y{0};
};
[[nodiscard]] Lifted lift_selected(const Canvas& canvas, const Selection& selection);
// Mixes `image`, of the canvas's format, over the canvas with its top left corner at x, y; returns the tiles written.
std::vector<TileIndex> lay_over(Canvas& canvas, const Image& image, int x, int y);

} // namespace olagarro::paint
