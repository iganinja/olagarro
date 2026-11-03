#pragma once

#include <paint/canvas.h>
#include <paint/geometry.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace olagarro::paint
{

// How a new shape changes the selection there is: Paint.NET's modes.
enum class SelectionMode
{
    replace,
    add,
    subtract,
    intersect,
    // What was selected is not, and what was not is.
    invert,
};

// What part of the image tools work on: how much each pixel is selected, from 0 to 255, kept in tiles as a grey canvas
// whose grey is that amount, so keeping it for undo costs what a canvas does. No selection is all the image.
class Selection
{
  public:
    Selection(int width, int height);

    // The image's size.
    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    // Whether anything is selected; with nothing, tools work on the whole image.
    [[nodiscard]] bool active() const { return m_mask.has_value(); }
    // How much the pixel is selected, 255 everywhere when nothing is.
    [[nodiscard]] std::uint8_t at(int x, int y) const;
    // The smallest rectangle holding every pixel selected at all; the whole image when nothing is.
    [[nodiscard]] PixelArea bounds() const;

    void clear() { m_mask.reset(); }
    void select_all();
    void invert();
    // Combines a rectangle, or the ellipse inside it with an antialiased edge, with what is selected.
    void add_rectangle(const PixelArea& area, SelectionMode mode);
    void add_ellipse(double left, double top, double right, double bottom, SelectionMode mode);
    // Combines the polygon through `points`, closed, inside by the even-odd rule, with an antialiased edge.
    void add_polygon(const std::vector<Position>& points, SelectionMode mode);
    // Combines a coverage for every pixel of the image, row by row, 0 to 255, as a magic wand's region.
    void add_coverage(const std::vector<std::uint8_t>& coverage, SelectionMode mode);
    // The selection moved by whole pixels.
    void offset(int dx, int dy);

    [[nodiscard]] const std::optional<Canvas>& mask() const { return m_mask; }
    void set_mask(std::optional<Canvas> mask) { m_mask = std::move(mask); }

  private:
    // Combines a shape whose coverage at each pixel of `area` is `cover(x, y)`, from 0 to 1.
    template <typename Cover> void combine(const PixelArea& area, SelectionMode mode, Cover cover);
    void set(int x, int y, std::uint8_t value);
    // A mask with nothing selected.
    void empty_mask();

    int m_width;
    int m_height;
    std::optional<Canvas> m_mask;
};

} // namespace olagarro::paint
