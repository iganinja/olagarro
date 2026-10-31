#pragma once

#include <oui/container.h>

#include <memory>

namespace olagarro::oui
{

class Row final : public Container
{
  public:
    void set_gap(Spacing gap);
    // A gap of `Length::cells` or `Length::pixels` (across); other kinds are none.
    void set_gap(Length gap);
    void set_centered(bool centered);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;

  private:
    Length m_gap{Length::cells(0)};
    bool m_centered{false};
};

class Column final : public Container
{
  public:
    void set_gap(Spacing gap);
    // A gap of `Length::cells` or `Length::pixels` (down); other kinds are none.
    void set_gap(Length gap);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;

  private:
    Length m_gap{Length::cells(0)};
};

[[nodiscard]] std::unique_ptr<Row> row();
Row& row(Container& parent);
[[nodiscard]] std::unique_ptr<Column> column();
Column& column(Container& parent);

} // namespace olagarro::oui
