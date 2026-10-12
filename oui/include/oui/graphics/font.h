#pragma once

#include <oui/graphics/canvas.h>

#include <memory>
#include <string_view>

namespace olagarro::oui
{

class Font
{
  public:
    virtual ~Font() = default;

    // The glyph for a character, or the font's replacement when it has none.
    [[nodiscard]] virtual const GlyphBitmap& glyph(char32_t character) = 0;
    [[nodiscard]] virtual bool has_glyph(char32_t character) const = 0;
    // From the top of a line to its baseline.
    [[nodiscard]] virtual int ascent() const = 0;
    [[nodiscard]] virtual int line_height() const = 0;

    // The width of a run of UTF-8 text, in pixels.
    [[nodiscard]] int measure(std::string_view text);
    // Draws a run of UTF-8 text with the top of its line at `top`; returns where the pen ends.
    int draw(Canvas& canvas, int x, int top, std::string_view text, Color color);
};

// The three fonts a graphical style writes with: cells in `cell`, interface text in `interface`, and what either
// lacks, such as arrows, in `symbols`.
struct FontSet
{
    Font& cell;
    Font& interface;
    Font& symbols;

    [[nodiscard]] const GlyphBitmap& cell_glyph(char32_t character) const;
    // A character the interface font lacks, such as a box line, comes from the cell font, and an arrow from the
    // symbols, on the same baseline.
    [[nodiscard]] const GlyphBitmap& interface_glyph(char32_t character) const;
    [[nodiscard]] int interface_width(std::string_view text) const;
};

// A bitmap font in the PC Screen Font 2 format, as the Linux console uses, with its Unicode table: fixed width.
// `scale` makes every pixel a square of that many, for sizes the font does not come in.
[[nodiscard]] std::unique_ptr<Font> psf_font(std::string_view bytes, int scale = 1);
// A TrueType or OpenType font at `pixel_height`, from the top of its ascent to the bottom of its descent. The
// bytes must outlive the font.
[[nodiscard]] std::unique_ptr<Font> truetype_font(std::string_view bytes, float pixel_height, int extra_leading = 0);

// Arrows, triangles and squares that interface text and cells use and pixel fonts often lack, drawn as shapes for
// cells of `cell` pixels with the baseline at `ascent`.
[[nodiscard]] std::unique_ptr<Font> symbol_font(int cell_width, int cell_height, int ascent);

// The fonts built into OUI: Spleen for the cells (BSD 2-clause), 8x16, 12x24 and 16x32, and the two larger doubled
// (8x16 doubled would be the size of 16x32), and Jersey 10 for interface text (SIL Open Font License); their licences
// are in oui/fonts. The defaults are 12x24 and 24 pixels.
[[nodiscard]] std::unique_ptr<Font> built_in_cell_font(int cell_height);
[[nodiscard]] std::unique_ptr<Font> built_in_interface_font(float pixels);
[[nodiscard]] std::unique_ptr<Font> default_cell_font();
[[nodiscard]] std::unique_ptr<Font> default_interface_font();

} // namespace olagarro::oui
