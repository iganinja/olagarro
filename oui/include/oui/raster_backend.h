#pragma once

#include <oui/backend.h>

#include <optional>
#include <vector>

namespace olagarro::oui
{

class RasterBackend final : public Backend
{
  public:
    RasterBackend(Size cells, int cell_width = 8, int cell_height = 16);
    [[nodiscard]] Size size() const override;
    std::optional<Event> poll_event() override;
    void present(const CellBuffer& buffer) override;
    [[nodiscard]] int pixel_width() const;
    [[nodiscard]] int pixel_height() const;
    [[nodiscard]] const Color& pixel_at(int x, int y) const;

  private:
    Size m_cells;
    int m_cell_width;
    int m_cell_height;
    std::vector<Color> m_pixels;
};

} // namespace olagarro::oui
