#include "stb_truetype_api.h"

#include <cmath>
#include <stdexcept>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace olagarro::oui::stbtt_api
{

struct FontInfo
{
    stbtt_fontinfo info{};
};

std::shared_ptr<FontInfo> load(std::string_view bytes)
{
    auto font{std::make_shared<FontInfo>()};
    const auto* const data{reinterpret_cast<const unsigned char*>(bytes.data())};
    if(stbtt_InitFont(&font->info, data, stbtt_GetFontOffsetForIndex(data, 0)) == 0)
    {
        throw std::invalid_argument{"This is not a TrueType or OpenType font."};
    }
    return font;
}

float scale_for_pixel_height(const FontInfo& font, float pixels)
{
    return stbtt_ScaleForPixelHeight(&font.info, pixels);
}

VerticalMetrics vertical_metrics(const FontInfo& font)
{
    VerticalMetrics metrics;
    stbtt_GetFontVMetrics(&font.info, &metrics.ascent, &metrics.descent, &metrics.line_gap);
    return metrics;
}

int glyph_index(const FontInfo& font, char32_t character)
{
    return stbtt_FindGlyphIndex(&font.info, static_cast<int>(character));
}

GlyphBitmap render(const FontInfo& font, int glyph, float scale)
{
    GlyphBitmap result;
    int advance{};
    int bearing{};
    stbtt_GetGlyphHMetrics(&font.info, glyph, &advance, &bearing);
    result.advance = static_cast<int>(std::lround(static_cast<float>(advance) * scale));
    auto* const bitmap{stbtt_GetGlyphBitmap(&font.info, scale, scale, glyph, &result.width, &result.height,
                                            &result.left, &result.top)};
    if(bitmap != nullptr)
    {
        result.coverage.assign(bitmap, bitmap + result.width * result.height);
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    return result;
}

} // namespace olagarro::oui::stbtt_api
