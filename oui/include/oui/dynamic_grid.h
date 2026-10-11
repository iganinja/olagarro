#pragma once

#include <oui/container.h>

#include <memory>

namespace olagarro::oui
{

class DynamicGrid final : public Container
{
  public:
    void set_column_width(Length width);
    void set_gap(Spacing gap);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;

  private:
    [[nodiscard]] Size cell_size(const Constraints& constraints) const;
    [[nodiscard]] int column_count(int available_width, int cell_width) const;
    Length m_column_width{Length::content()};
    Spacing m_gap{Spacing::none};
};

[[nodiscard]] std::unique_ptr<DynamicGrid> dynamic_grid();
DynamicGrid& dynamic_grid(Container& parent);

} // namespace olagarro::oui
