#include <oui/graphics/pixel_backend.h>

#include <oui/file_system.h>
#include <oui/trace.h>
#include <oui/utf8.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <cstdio>
#include <stdexcept>

namespace olagarro::oui
{

namespace
{

class PixelTextMetrics final : public TextMetrics
{
  public:
    PixelTextMetrics(FontSet fonts, Size cell) : m_fonts{fonts}, m_cell{cell} {}

    int cell_width() const override { return whole(m_cell.width); }
    int width(std::string_view text) const override { return m_fonts.interface_width(text); }
    int field_padding() const override { return GraphicalPainter::field_padding(m_cell); }
    int popup_frame_rows() const override { return 0; }
    int panel_gap() const override { return 1; }
    int window_top_rows() const override { return 2; }
    int window_side_columns() const override { return 1; }
    bool window_close_button() const override { return true; }
    std::size_t byte_at(std::string_view text, int position, bool) const override
    {
        // The nearer edge of the character under the position.
        std::size_t index{};
        auto x{0};
        while(index < text.size())
        {
            const auto start{index};
            const auto advance{m_fonts.interface_glyph(decode_utf8(text, index)).advance};
            if(position < x + (advance + 1) / 2)
            {
                return start;
            }
            x += advance;
        }
        return text.size();
    }

  private:
    FontSet m_fonts;
    Size m_cell;
};

} // namespace

namespace
{

// Each size: the cell font's height and the interface font's, the same: Jersey 10's lines hold all its ink, so it
// fills the row without spilling out of it, and the interface looks as big as the editor's text beside it.
struct ZoomLevel
{
    int cell_height;
    float interface_pixels;
};

constexpr ZoomLevel zoom_table[]{{16, 16.0f}, {24, 24.0f}, {32, 32.0f}, {48, 48.0f}, {64, 64.0f}};

} // namespace

PixelBackend::PixelBackend(GraphicalStyle style) : m_style{style}, m_zoom{default_zoom()}, m_canvas{0, 0}
{
    load_fonts();
}

void PixelBackend::load_fonts()
{
    const auto& level{zoom_table[m_zoom]};
    // The metrics in use refer to the fonts about to go.
    if(m_metrics != nullptr && &text_metrics() == m_metrics.get())
    {
        set_text_metrics(nullptr);
    }
    m_cell_font = built_in_cell_font(level.cell_height);
    if(m_style.interface_font == InterfaceFont::noto_sans)
    {
        // Noto Sans fills the height it is given with room for accents above and tails below, where Jersey 10's lines
        // hold only its ink; at a row's height its text would sit on the row's lower edge, so the rows are taller.
        m_cell_font = spaced_font(std::move(m_cell_font), level.cell_height / 6);
    }
    m_interface_font = built_in_interface_font(level.interface_pixels, m_style.interface_font);
    m_cell = GraphicalPainter::cell_size(*m_cell_font);
    m_symbols = symbol_font(whole(m_cell.width), whole(m_cell.height), m_cell_font->ascent());
    m_metrics = std::make_unique<PixelTextMetrics>(FontSet{*m_cell_font, *m_interface_font, *m_symbols}, m_cell);
    set_text_metrics(m_metrics.get());
}

bool PixelBackend::zooms() const
{
    return true;
}

bool PixelBackend::zoom_by(int step)
{
    set_zoom(m_zoom + step);
    return true;
}

bool PixelBackend::zoom_reset()
{
    set_zoom(default_zoom());
    return true;
}

void PixelBackend::set_zoom(int level)
{
    const auto clamped{std::clamp(level, 0, zoom_levels() - 1)};
    if(clamped == m_zoom)
    {
        return;
    }
    m_zoom = clamped;
    load_fonts();
    fonts_changed();
}

int PixelBackend::zoom() const
{
    return m_zoom;
}

int PixelBackend::zoom_levels()
{
    return static_cast<int>(std::size(zoom_table));
}

int PixelBackend::columns_covered(int cell_offset, int cell_width, int pointer_size)
{
    const auto reach{(pointer_size * 3 + 4) / 5};
    return std::max(1, (std::max(0, cell_offset) + reach - 1) / std::max(1, cell_width) + 1);
}

int PixelBackend::pointer_columns(int cell_offset) const
{
    return columns_covered(cell_offset, whole(cell_size().width), pointer_size());
}

int PixelBackend::default_zoom()
{
    return 1;
}

void PixelBackend::fonts_changed()
{
}

Font& PixelBackend::interface_font()
{
    return *m_interface_font;
}

PixelBackend::~PixelBackend()
{
    if(&text_metrics() == m_metrics.get())
    {
        set_text_metrics(nullptr);
    }
}

Size PixelBackend::size() const
{
    return {std::max(0.0, std::floor(m_content.width / m_cell.width)),
            std::max(0.0, std::floor(m_content.height / m_cell.height))};
}

void PixelBackend::render(const std::function<void(Painter&)>& paint)
{
    m_canvas.set_clip({0, 0, m_canvas.width(), m_canvas.height()});
    {
        const TraceScope span{"oui.paint"};
        m_canvas.fill({0, 0, m_canvas.width(), m_canvas.height()}, m_style.desktop);
        GraphicalPainter painter{m_canvas, m_style, *m_cell_font, *m_interface_font, *m_symbols, m_content};
        paint(painter);
    }
    redecorate();
}

void PixelBackend::redecorate()
{
    m_canvas.set_clip({0, 0, m_canvas.width(), m_canvas.height()});
    {
        const TraceScope span{"oui.decorate"};
        decorate(m_canvas, m_style, *m_interface_font);
    }
    const TraceScope span{"oui.present"};
    show(m_canvas);
}

void PixelBackend::decorate(Canvas&, const GraphicalStyle&, Font&)
{
}

const PixelRect& PixelBackend::content_area() const
{
    return m_content;
}

void PixelBackend::set_content_area(const PixelRect& area)
{
    m_content = intersect(area, {0, 0, m_canvas.width(), m_canvas.height()});
}

void PixelBackend::present(const CellBuffer& buffer)
{
    render(
        [&buffer](Painter& painter)
        {
            for(int y{}; y < buffer.height(); ++ y)
            {
                for(int x{}; x < buffer.width(); ++ x)
                {
                    const auto& cell{buffer.cell_at(x, y)};
                    if(cell.continuation)
                    {
                        continue;
                    }
                    painter.draw_text({static_cast<double>(x), static_cast<double>(y)}, encode_utf8(cell.glyph),
                                      {cell.foreground, cell.background, cell.attributes});
                }
            }
        });
}

void PixelBackend::set_style(GraphicalStyle style)
{
    const auto font_changed{style.interface_font != m_style.interface_font};
    m_style = style;
    if(font_changed)
    {
        load_fonts();
        fonts_changed();
    }
}

const GraphicalStyle& PixelBackend::style() const
{
    return m_style;
}

Size PixelBackend::cell_size() const
{
    return m_cell;
}

const Canvas& PixelBackend::canvas() const
{
    return m_canvas;
}

void PixelBackend::resize(int width, int height)
{
    m_canvas.resize(width, height);
    m_content = {0, 0, width, height};
}

ImageBackend::ImageBackend(Size pixels, std::vector<Event> events) : m_events{std::move(events)}
{
    resize(whole(pixels.width), whole(pixels.height));
}

void ImageBackend::push(const Event& event)
{
    m_events.push_back(event);
}

std::optional<Event> ImageBackend::poll_event()
{
    if(m_next >= m_events.size())
    {
        return std::nullopt;
    }
    return m_events[m_next++];
}

std::size_t ImageBackend::frame_count() const
{
    return m_frame_count;
}

void ImageBackend::show(const Canvas&)
{
    ++ m_frame_count;
}

std::unique_ptr<ImageBackend> image_backend(Size pixels, std::vector<Event> events)
{
    return std::make_unique<ImageBackend>(pixels, std::move(events));
}

namespace
{

class Crc
{
  public:
    Crc()
    {
        for(std::uint32_t n{}; n < 256; ++ n)
        {
            auto c{n};
            for(int k{}; k < 8; ++ k)
            {
                c = (c & 1u) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            m_table[n] = c;
        }
    }
    [[nodiscard]] std::uint32_t of(const std::string& bytes) const
    {
        std::uint32_t c{0xFFFFFFFFu};
        for(const auto byte : bytes)
        {
            c = m_table[(c ^ static_cast<unsigned char>(byte)) & 0xFFu] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
    }

  private:
    std::array<std::uint32_t, 256> m_table{};
};

void put_u32(std::string& out, std::uint32_t value)
{
    for(int shift{24}; shift >= 0; shift -= 8)
    {
        out += static_cast<char>((value >> shift) & 0xFFu);
    }
}

} // namespace

void write_png(const Canvas& canvas, const std::filesystem::path& path)
{
    // Rows of RGB, each after a filter byte of 0, in deflate blocks that store them as they are.
    std::string raw;
    raw.reserve(static_cast<std::size_t>((canvas.width() * 3 + 1) * canvas.height()));
    for(int y{}; y < canvas.height(); ++ y)
    {
        raw += '\0';
        for(int x{}; x < canvas.width(); ++ x)
        {
            const auto pixel{canvas.pixels()[static_cast<std::size_t>(y * canvas.width() + x)]};
            raw += static_cast<char>((pixel >> 16) & 0xFFu);
            raw += static_cast<char>((pixel >> 8) & 0xFFu);
            raw += static_cast<char>(pixel & 0xFFu);
        }
    }
    std::string zlib{"\x78\x01", 2};
    for(std::size_t offset{}; offset < raw.size() || offset == 0;)
    {
        const auto length{std::min<std::size_t>(65535, raw.size() - offset)};
        const auto last{offset + length >= raw.size()};
        zlib += static_cast<char>(last ? 1 : 0);
        zlib += static_cast<char>(length & 0xFFu);
        zlib += static_cast<char>(length >> 8);
        zlib += static_cast<char>(~length & 0xFFu);
        zlib += static_cast<char>((~length >> 8) & 0xFFu);
        zlib.append(raw, offset, length);
        offset += length;
        if(last)
        {
            break;
        }
    }
    std::uint32_t a{1};
    std::uint32_t b{};
    for(const auto byte : raw)
    {
        a = (a + static_cast<unsigned char>(byte)) % 65521u;
        b = (b + a) % 65521u;
    }
    put_u32(zlib, (b << 16) | a);

    const Crc crc;
    std::string png{"\x89PNG\r\n\x1a\n", 8};
    const auto chunk{[&](const char* type, const std::string& data)
                     {
                         put_u32(png, static_cast<std::uint32_t>(data.size()));
                         const auto body{std::string{type, 4} + data};
                         png += body;
                         put_u32(png, crc.of(body));
                     }};
    std::string header;
    put_u32(header, static_cast<std::uint32_t>(canvas.width()));
    put_u32(header, static_cast<std::uint32_t>(canvas.height()));
    header += std::string{"\x08\x02\x00\x00\x00", 5};
    chunk("IHDR", header);
    chunk("IDAT", zlib);
    chunk("IEND", {});
    auto* const file{open_file(path, "wb")};
    const auto written{file != nullptr && std::fwrite(png.data(), 1, png.size(), file) == png.size()};
    if((file != nullptr && std::fclose(file) != 0) || !written)
    {
        throw std::runtime_error{"Cannot write " + utf8_of(path)};
    }
}

} // namespace olagarro::oui
