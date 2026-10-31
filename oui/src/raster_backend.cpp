#include <oui/raster_backend.h>

#include <algorithm>

namespace olagarro::oui
{

RasterBackend::RasterBackend(Size cells, int cell_width, int cell_height)
    : m_cells{cells}, m_cell_width{cell_width}, m_cell_height{cell_height},
      m_pixels(static_cast<std::size_t>(whole(cells.width) * cell_width * whole(cells.height) * cell_height))
{
}

Size RasterBackend::size() const
{
    return m_cells;
}
std::optional<Event> RasterBackend::poll_event()
{
    return std::nullopt;
}
void RasterBackend::present(const CellBuffer& buffer)
{
    for(int y{}; y < std::min(whole(m_cells.height), buffer.height()); ++ y)
    {
        for(int x{}; x < std::min(whole(m_cells.width), buffer.width()); ++ x)
        {
            const auto color{buffer.cell_at(x, y).background};
            for(int pixel_y{}; pixel_y < m_cell_height; ++ pixel_y)
            {
                for(int pixel_x{}; pixel_x < m_cell_width; ++ pixel_x)
                {
                    m_pixels.at(static_cast<std::size_t>((y * m_cell_height + pixel_y) * pixel_width() +
                                                         x * m_cell_width + pixel_x)) = color;
                }
            }
        }
    }
}
int RasterBackend::pixel_width() const
{
    return whole(m_cells.width) * m_cell_width;
}
int RasterBackend::pixel_height() const
{
    return whole(m_cells.height) * m_cell_height;
}
const Color& RasterBackend::pixel_at(int x, int y) const
{
    return m_pixels.at(static_cast<std::size_t>(y * pixel_width() + x));
}

} // namespace olagarro::oui
