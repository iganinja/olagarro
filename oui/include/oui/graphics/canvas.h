#pragma once

#include <oui/color.h>

#include <cstdint>
#include <vector>

namespace olagarro::oui
{

struct PixelRect
{
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

struct PixelPoint
{
    float x{0.0f};
    float y{0.0f};
};

// A glyph as coverage, 0 to 255, placed from the pen on the baseline: `left` to the right and `top` below it (negative,
// as glyphs rise above the baseline).
struct GlyphBitmap
{
    int width{0};
    int height{0};
    int left{0};
    int top{0};
    int advance{0};
    std::vector<std::uint8_t> coverage;
};

// Pixels in memory, as 0x00RRGGBB, that every drawing operation clips to the current clip rectangle.
class Canvas
{
  public:
    Canvas(int width, int height);

    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const;
    [[nodiscard]] Color pixel_at(int x, int y) const;
    // Keeps nothing of what was drawn.
    void resize(int width, int height);

    void set_clip(const PixelRect& clip);
    [[nodiscard]] const PixelRect& clip() const;

    void fill(const PixelRect& rect, Color color);
    // Mixes `color` over a rectangle by `alpha`, 0 to 255.
    void fill_blended(const PixelRect& rect, Color color, int alpha);
    // Mixes `color` into a pixel by `alpha`, 0 to 255.
    void blend(int x, int y, Color color, int alpha);
    // Antialiased: each pixel takes the share of it inside the polygon, by the even-odd rule.
    void fill_polygon(const std::vector<PixelPoint>& points, Color color);
    // A stroke of `width` pixels between two points, antialiased.
    void draw_line(PixelPoint from, PixelPoint to, float width, Color color);
    // Copies `width` by `height` pixels, each row `stride` after the one before, with their top left corner at x, y.
    void copy(int x, int y, const std::uint32_t* pixels, int width, int height, int stride);
    // As copy, but the pixels are 0xAARRGGBB with straight alpha and are mixed over what is there by it.
    void mix_over(int x, int y, const std::uint32_t* pixels, int width, int height, int stride);
    // `alpha` makes the glyph see-through, 255 being solid.
    void draw_glyph(const GlyphBitmap& glyph, int pen_x, int baseline, Color color, int alpha = 255);

  private:
    int m_width;
    int m_height;
    std::vector<std::uint32_t> m_pixels;
    PixelRect m_clip;
};

[[nodiscard]] PixelRect intersect(const PixelRect& first, const PixelRect& second);

} // namespace olagarro::oui
