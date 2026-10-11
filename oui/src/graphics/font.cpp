#include <oui/graphics/font.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cstring>
#include <string_view>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "stb_truetype_api.h"

namespace olagarro::oui
{

std::vector<std::string_view> embedded_oui_fonts();

namespace
{

[[nodiscard]] std::uint32_t read_u32(std::string_view bytes, std::size_t offset)
{
    if(offset + 4 > bytes.size())
    {
        throw std::invalid_argument{"The font ends too soon."};
    }
    std::uint32_t value{};
    for(int index{3}; index >= 0; -- index)
    {
        value = (value << 8) | static_cast<unsigned char>(bytes[offset + static_cast<std::size_t>(index)]);
    }
    return value;
}

class PsfFont final : public Font
{
  public:
    PsfFont(std::string_view bytes, int scale)
    {
        // PSF1, the older form, holds 256 or 512 glyphs 8 pixels wide, its table in UTF-16; PSF2 says it all.
        const auto version_1{bytes.size() >= 4 && static_cast<unsigned char>(bytes[0]) == 0x36 &&
                             static_cast<unsigned char>(bytes[1]) == 0x04};
        if(!version_1 && read_u32(bytes, 0) != 0x864ab572u)
        {
            throw std::invalid_argument{"This is not a PSF font."};
        }
        const auto mode{version_1 ? static_cast<unsigned char>(bytes[2]) : 0u};
        const auto header_size{version_1 ? 4u : read_u32(bytes, 8)};
        const auto has_table{version_1 ? (mode & 0x06u) != 0 : (read_u32(bytes, 12) & 1u) != 0};
        const auto count{version_1 ? ((mode & 0x01u) != 0 ? 512u : 256u) : read_u32(bytes, 16)};
        const auto glyph_size{version_1 ? static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]))
                                        : read_u32(bytes, 20)};
        m_height = version_1 ? static_cast<int>(glyph_size) : static_cast<int>(read_u32(bytes, 24));
        m_width = version_1 ? 8 : static_cast<int>(read_u32(bytes, 28));
        const auto row_bytes{static_cast<std::size_t>((m_width + 7) / 8)};
        const auto table{static_cast<std::size_t>(header_size) + std::size_t{count} * glyph_size};
        if(table > bytes.size() || row_bytes * static_cast<std::size_t>(m_height) > glyph_size)
        {
            throw std::invalid_argument{"The PSF font is shorter than its header says."};
        }
        m_width *= scale;
        m_height *= scale;
        m_glyphs.resize(count);
        for(std::uint32_t index{}; index < count; ++ index)
        {
            auto& glyph{m_glyphs[index]};
            glyph.width = m_width;
            glyph.height = m_height;
            glyph.left = 0;
            glyph.top = -ascent();
            glyph.advance = m_width;
            glyph.coverage.resize(static_cast<std::size_t>(m_width * m_height));
            const auto* const data{bytes.data() + header_size + std::size_t{index} * glyph_size};
            for(int y{}; y < m_height; ++ y)
            {
                for(int x{}; x < m_width; ++ x)
                {
                    // Each pixel of the font a square of `scale` pixels.
                    const auto column{x / scale};
                    const auto byte{static_cast<unsigned char>(
                        data[static_cast<std::size_t>(y / scale) * row_bytes + static_cast<std::size_t>(column / 8)])};
                    glyph.coverage[static_cast<std::size_t>(y * m_width + x)] =
                        (byte & (0x80 >> (column % 8))) != 0 ? 255 : 0;
                }
            }
        }
        // The Unicode table: for each glyph, the characters it shows, then an end mark; another mark starts sequences
        // of several characters, which a cell never asks for.
        if(!has_table)
        {
            for(std::uint32_t index{}; index < count; ++ index)
            {
                m_index[index] = index;
            }
            return;
        }
        auto position{table};
        if(version_1)
        {
            // Little-endian UTF-16 units; 0xFFFF ends a glyph's entry, 0xFFFE starts a sequence.
            for(std::uint32_t index{}; index < count && position + 1 < bytes.size(); ++ index)
            {
                bool in_sequence{false};
                while(position + 1 < bytes.size())
                {
                    const auto unit{static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[position])) |
                                    (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[position + 1])) << 8)};
                    position += 2;
                    if(unit == 0xFFFFu)
                    {
                        break;
                    }
                    if(unit == 0xFFFEu)
                    {
                        in_sequence = true;
                    }
                    else if(!in_sequence)
                    {
                        m_index.emplace(static_cast<char32_t>(unit), index);
                    }
                }
            }
            return;
        }
        for(std::uint32_t index{}; index < count && position < bytes.size(); ++ index)
        {
            bool in_sequence{false};
            while(position < bytes.size() && static_cast<unsigned char>(bytes[position]) != 0xFF)
            {
                if(static_cast<unsigned char>(bytes[position]) == 0xFE)
                {
                    in_sequence = true;
                    ++ position;
                    continue;
                }
                const auto character{decode_utf8(bytes, position)};
                if(!in_sequence)
                {
                    m_index.emplace(character, index);
                }
            }
            ++ position;
        }
    }

    const GlyphBitmap& glyph(char32_t character) override
    {
        if(const auto found{m_index.find(character)}; found != m_index.end())
        {
            return m_glyphs[found->second];
        }
        if(const auto replacement{m_index.find(U'�')}; replacement != m_index.end())
        {
            return m_glyphs[replacement->second];
        }
        return m_glyphs[m_index.at(U'?')];
    }
    // PSF says nothing of the baseline; Spleen, at every size, puts it about a fifth of the height from the bottom.
    int ascent() const override { return m_height * 4 / 5; }
    bool has_glyph(char32_t character) const override { return m_index.count(character) != 0; }
    int line_height() const override { return m_height; }

  private:
    int m_width{};
    int m_height{};
    std::vector<GlyphBitmap> m_glyphs;
    std::unordered_map<char32_t, std::uint32_t> m_index;
};

class TrueTypeFont final : public Font
{
  public:
    TrueTypeFont(std::string_view bytes, float pixel_height, int extra_leading)
        : m_info{stbtt_api::load(bytes)}, m_scale{stbtt_api::scale_for_pixel_height(*m_info, pixel_height)}
    {
        const auto metrics{stbtt_api::vertical_metrics(*m_info)};
        m_ascent = static_cast<int>(static_cast<float>(metrics.ascent) * m_scale + 0.5f);
        m_line_height =
            m_ascent + static_cast<int>(static_cast<float>(-metrics.descent) * m_scale + 0.5f) + extra_leading;
    }

    const GlyphBitmap& glyph(char32_t character) override
    {
        auto [found, inserted]{m_glyphs.try_emplace(character)};
        if(inserted)
        {
            auto index{stbtt_api::glyph_index(*m_info, character)};
            if(index == 0)
            {
                index = stbtt_api::glyph_index(*m_info, U'?');
            }
            found->second = stbtt_api::render(*m_info, index, m_scale);
        }
        return found->second;
    }
    int ascent() const override { return m_ascent; }
    bool has_glyph(char32_t character) const override { return stbtt_api::glyph_index(*m_info, character) != 0; }
    int line_height() const override { return m_line_height; }

  private:
    std::shared_ptr<stbtt_api::FontInfo> m_info;
    float m_scale;
    int m_ascent{};
    int m_line_height{};
    std::unordered_map<char32_t, GlyphBitmap> m_glyphs;
};

// Shapes in a box of the cell's size, from points in units of that box.
class SymbolFont final : public Font
{
  public:
    SymbolFont(int width, int height, int ascent) : m_width{width}, m_height{height}, m_ascent{ascent} {}

    const GlyphBitmap& glyph(char32_t character) override
    {
        auto [found, inserted]{m_glyphs.try_emplace(character)};
        if(inserted)
        {
            found->second = draw(has_glyph(character) ? character : U'■');
        }
        return found->second;
    }
    bool has_glyph(char32_t character) const override
    {
        return std::u32string_view{U"←↑→↓■□▲▶▼◀▴▸▾◂√"}.find(character) != std::u32string_view::npos;
    }
    int ascent() const override { return m_ascent; }
    int line_height() const override { return m_height; }

  private:
    [[nodiscard]] GlyphBitmap draw(char32_t character) const
    {
        Canvas canvas{m_width, m_height};
        const auto w{static_cast<float>(m_width)};
        // Shapes sit on the middle of the lower-case letters.
        const auto middle{static_cast<float>(m_ascent) * 0.62f};
        const auto size{std::min(w * 0.8f, static_cast<float>(m_height) * 0.45f)};
        const auto cx{w / 2};
        const Color ink{255, 255, 255};
        const auto at{[&](float x, float y) { return PixelPoint{cx + x * size / 2, middle + y * size / 2}; }};
        const auto arrow{
            [&](float dx, float dy)
            {
                // A shaft and a head, pointing along (dx, dy).
                const auto px{-dy};
                const auto py{dx};
                canvas.draw_line(at(-dx * 0.9f, -dy * 0.9f), at(dx * 0.3f, dy * 0.3f), size * 0.14f, ink);
                canvas.fill_polygon({at(dx * 0.95f, dy * 0.95f), at(dx * 0.1f + px * 0.6f, dy * 0.1f + py * 0.6f),
                                     at(dx * 0.1f - px * 0.6f, dy * 0.1f - py * 0.6f)},
                                    ink);
            }};
        const auto triangle{
            [&](float dx, float dy, float scale)
            {
                const auto px{-dy};
                const auto py{dx};
                canvas.fill_polygon({at(dx * scale, dy * scale), at((-dx + px) * scale, (-dy + py) * scale),
                                     at((-dx - px) * scale, (-dy - py) * scale)},
                                    ink);
            }};
        switch(character)
        {
        case U'√':
        {
            // A short rise, down to the line the letters stand on, then a long stroke up to the top of the capitals,
            // as a radical is written.
            const auto a{static_cast<float>(m_ascent)};
            const auto stroke{size * 0.14f};
            canvas.draw_line({w * 0.08f, a * 0.62f}, {w * 0.2f, a * 0.55f}, stroke, ink);
            canvas.draw_line({w * 0.2f, a * 0.55f}, {w * 0.35f, a * 0.97f}, stroke, ink);
            canvas.draw_line({w * 0.35f, a * 0.97f}, {w * 0.92f, a * 0.2f}, stroke, ink);
            break;
        }
        case U'←':
            arrow(-1, 0);
            break;
        case U'↑':
            arrow(0, -1);
            break;
        case U'→':
            arrow(1, 0);
            break;
        case U'↓':
            arrow(0, 1);
            break;
        case U'■':
            canvas.fill_polygon({at(-0.6f, -0.6f), at(0.6f, -0.6f), at(0.6f, 0.6f), at(-0.6f, 0.6f)}, ink);
            break;
        case U'□':
            canvas.fill_polygon({at(-0.6f, -0.6f), at(0.6f, -0.6f), at(0.6f, 0.6f), at(-0.6f, 0.6f), at(-0.6f, -0.6f),
                                 at(-0.45f, -0.45f), at(-0.45f, 0.45f), at(0.45f, 0.45f), at(0.45f, -0.45f),
                                 at(-0.45f, -0.45f)},
                                ink);
            break;
        case U'▲':
            triangle(0, -1, 0.8f);
            break;
        case U'▶':
            triangle(1, 0, 0.8f);
            break;
        case U'▼':
            triangle(0, 1, 0.8f);
            break;
        case U'◀':
            triangle(-1, 0, 0.8f);
            break;
        case U'▴':
            triangle(0, -1, 0.5f);
            break;
        case U'▸':
            triangle(1, 0, 0.5f);
            break;
        case U'▾':
            triangle(0, 1, 0.5f);
            break;
        case U'◂':
            triangle(-1, 0, 0.5f);
            break;
        default:
            break;
        }
        GlyphBitmap glyph;
        glyph.width = m_width;
        glyph.height = m_height;
        glyph.top = -m_ascent;
        glyph.advance = m_width;
        for(const auto pixel : canvas.pixels())
        {
            glyph.coverage.push_back(static_cast<std::uint8_t>(pixel & 0xFFu));
        }
        return glyph;
    }

    int m_width;
    int m_height;
    int m_ascent;
    std::unordered_map<char32_t, GlyphBitmap> m_glyphs;
};

} // namespace

const GlyphBitmap& FontSet::cell_glyph(char32_t character) const
{
    if(!cell.has_glyph(character) && symbols.has_glyph(character))
    {
        return symbols.glyph(character);
    }
    return cell.glyph(character);
}

const GlyphBitmap& FontSet::interface_glyph(char32_t character) const
{
    // A tab in interface text is a gap, as a space is: no font draws it.
    if(character == U'\t')
    {
        return interface.glyph(U' ');
    }
    if(interface.has_glyph(character))
    {
        return interface.glyph(character);
    }
    if(cell.has_glyph(character) || !symbols.has_glyph(character))
    {
        return cell_glyph(character);
    }
    return symbols.glyph(character);
}

int FontSet::interface_width(std::string_view text) const
{
    int width{};
    for(std::size_t index{}; index < text.size();)
    {
        width += interface_glyph(decode_utf8(text, index)).advance;
    }
    return width;
}

std::unique_ptr<Font> symbol_font(int cell_width, int cell_height, int ascent)
{
    return std::make_unique<SymbolFont>(cell_width, cell_height, ascent);
}

int Font::measure(std::string_view text)
{
    int width{};
    for(std::size_t index{}; index < text.size();)
    {
        width += glyph(decode_utf8(text, index)).advance;
    }
    return width;
}

int Font::draw(Canvas& canvas, int x, int top, std::string_view text, Color color)
{
    for(std::size_t index{}; index < text.size();)
    {
        const auto& shape{glyph(decode_utf8(text, index))};
        canvas.draw_glyph(shape, x, top + ascent(), color);
        x += shape.advance;
    }
    return x;
}

std::unique_ptr<Font> psf_font(std::string_view bytes, int scale)
{
    return std::make_unique<PsfFont>(bytes, scale);
}

std::unique_ptr<Font> truetype_font(std::string_view bytes, float pixel_height, int extra_leading)
{
    return std::make_unique<TrueTypeFont>(bytes, pixel_height, extra_leading);
}

std::unique_ptr<Font> built_in_cell_font(int cell_height)
{
    const auto fonts{embedded_oui_fonts()};
    switch(cell_height)
    {
    case 16:
        return psf_font(fonts.at(2));
    case 24:
        return psf_font(fonts.at(0));
    case 32:
        return psf_font(fonts.at(3));
    case 48:
        return psf_font(fonts.at(0), 2);
    case 64:
        return psf_font(fonts.at(3), 2);
    default:
        throw std::invalid_argument{"No built-in cell font is " + std::to_string(cell_height) + " pixels high."};
    }
}

std::unique_ptr<Font> built_in_interface_font(float pixels)
{
    return truetype_font(embedded_oui_fonts().at(1), pixels);
}

std::unique_ptr<Font> default_cell_font()
{
    return built_in_cell_font(24);
}

std::unique_ptr<Font> default_interface_font()
{
    return built_in_interface_font(20.0f);
}

} // namespace olagarro::oui
