#include <oui/cell_buffer.h>
#include <oui/utf8.h>

#include <algorithm>
#include <optional>
#include <stdexcept>

#include "unicode_composition.h"

namespace olagarro::oui
{
CellBuffer::CellBuffer(int width, int height)
    : m_width{width}, m_height{height}, m_cells(static_cast<std::size_t>(width * height))
{
    if(width < 0 || height < 0)
    {
        throw std::invalid_argument{"A cell buffer cannot have a negative size."};
    }
}
int CellBuffer::width() const
{
    return m_width;
}
int CellBuffer::height() const
{
    return m_height;
}
const Cell& CellBuffer::cell_at(int x, int y) const
{
    return m_cells.at(index_of(x, y));
}
void CellBuffer::set_cell(int x, int y, const Cell& cell)
{
    m_cells.at(index_of(x, y)) = cell;
}
void CellBuffer::clear(const Cell& cell)
{
    std::fill(m_cells.begin(), m_cells.end(), cell);
}
std::size_t CellBuffer::index_of(int x, int y) const
{
    if(x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        throw std::out_of_range{"Cell coordinates are outside the buffer."};
    }
    return static_cast<std::size_t>(y * m_width + x);
}
CellBufferPainter::CellBufferPainter(CellBuffer& buffer)
    : m_buffer{buffer}, m_clip{0, 0, buffer.width(), buffer.height()}
{
}
void CellBufferPainter::fill_rect(const Rect& rect, Color color)
{
    const auto clipped{intersect(rect, m_clip)};
    for(auto y{clipped.y}; y < clipped.y + clipped.height; ++ y)
    {
        for(auto x{clipped.x}; x < clipped.x + clipped.width; ++ x)
        {
            auto cell{m_buffer.cell_at(x, y)};
            // Blank: a shadow cast over it later shows what is there.
            cell.glyph = U' ';
            cell.continuation = false;
            cell.background = color;
            cell.foreground = color;
            m_buffer.set_cell(x, y, cell);
        }
    }
}
void CellBufferPainter::shade(const Rect& rect, Color foreground, Color background)
{
    const auto clipped{intersect(rect, m_clip)};
    for(auto y{clipped.y}; y < clipped.y + clipped.height; ++ y)
    {
        for(auto x{clipped.x}; x < clipped.x + clipped.width; ++ x)
        {
            auto cell{m_buffer.cell_at(x, y)};
            cell.foreground = foreground;
            cell.background = background;
            cell.attributes = 0;
            m_buffer.set_cell(x, y, cell);
        }
    }
}

void CellBufferPainter::draw_text(Point position, std::string_view text, const TextStyle& style)
{
    auto x{position.x};
    // The last cell written, so a combining accent that follows (NFD text) can merge into it.
    std::optional<Point> previous_cell;
    char32_t previous_glyph{};
    for(std::size_t index{}; index < text.size();)
    {
        const char32_t glyph{decode_utf8(text, index)};
        const auto width{terminal_cell_width(glyph)};
        if(width == 0 && previous_cell && is_diacritic(glyph))
        {
            if(const auto composed{compose(previous_glyph, glyph)})
            {
                previous_glyph = *composed;
                m_buffer.set_cell(previous_cell->x, previous_cell->y,
                                  {previous_glyph, style.foreground, style.background, style.attributes, false});
            }
            continue;
        }
        previous_cell.reset();
        if(width != 0 && contains(m_clip, {x, position.y}))
        {
            previous_cell = Point{x, position.y};
            previous_glyph = glyph;
            m_buffer.set_cell(x, position.y, {glyph, style.foreground, style.background, style.attributes, false});
            if(width == 2 && x + 1 < m_buffer.width() && contains(m_clip, {x + 1, position.y}))
            {
                m_buffer.set_cell(x + 1, position.y,
                                  {U' ', style.foreground, style.background, style.attributes, true});
            }
        }
        x += width;
    }
}
void CellBufferPainter::push_clip(const Rect& rect)
{
    m_clips.push_back(m_clip);
    m_clip = intersect(m_clip, rect);
}
void CellBufferPainter::pop_clip()
{
    if(m_clips.empty())
    {
        throw std::logic_error{"Cannot pop an empty clip stack."};
    }
    m_clip = m_clips.back();
    m_clips.pop_back();
}
bool CellBufferPainter::contains(const Rect& rect, Point point)
{
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height;
}
Rect CellBufferPainter::intersect(const Rect& first, const Rect& second)
{
    const auto left{std::max(first.x, second.x)};
    const auto top{std::max(first.y, second.y)};
    const auto right{std::min(first.x + first.width, second.x + second.width)};
    const auto bottom{std::min(first.y + first.height, second.y + second.height)};
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}
} // namespace olagarro::oui
