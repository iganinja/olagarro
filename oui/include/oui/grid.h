#pragma once

#include <oui/container.h>

#include <initializer_list>
#include <memory>
#include <vector>

namespace olagarro::oui
{

class Grid final : public Container
{
  public:
    void set_columns(std::vector<Length> columns);
    void set_columns(std::initializer_list<Length> columns);
    void set_rows(std::vector<Length> rows);
    void set_rows(std::initializer_list<Length> rows);
    void set_gap(Spacing gap);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    [[nodiscard]] Rect paint_bounds() const override;

  private:
    [[nodiscard]] std::vector<double> column_widths(double available_width, bool distribute_flexible = true) const;
    [[nodiscard]] std::vector<double> row_heights(const Constraints& constraints, bool distribute_flexible) const;
    // The columns, and the rows, with a widget shown in them: the others take no room.
    [[nodiscard]] std::vector<bool> shown_columns() const;
    [[nodiscard]] std::vector<bool> shown_rows() const;
    std::vector<Length> m_columns;
    std::vector<Length> m_rows;
    Spacing m_gap{Spacing::none};
};

[[nodiscard]] std::unique_ptr<Grid> grid();
Grid& grid(Container& parent);

} // namespace olagarro::oui
