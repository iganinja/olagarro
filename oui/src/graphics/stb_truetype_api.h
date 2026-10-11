#pragma once

#include <oui/graphics/canvas.h>

#include <memory>
#include <string_view>

// What OUI uses of stb_truetype, so its implementation builds in one file of its own, apart from OUI's warnings.
namespace olagarro::oui::stbtt_api
{

struct FontInfo;

struct VerticalMetrics
{
    int ascent{};
    int descent{};
    int line_gap{};
};

// The bytes must outlive the font.
[[nodiscard]] std::shared_ptr<FontInfo> load(std::string_view bytes);
[[nodiscard]] float scale_for_pixel_height(const FontInfo& font, float pixels);
[[nodiscard]] VerticalMetrics vertical_metrics(const FontInfo& font);
// 0 when the font has no glyph for the character.
[[nodiscard]] int glyph_index(const FontInfo& font, char32_t character);
[[nodiscard]] GlyphBitmap render(const FontInfo& font, int glyph, float scale);

} // namespace olagarro::oui::stbtt_api
