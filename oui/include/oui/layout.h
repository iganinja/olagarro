#pragma once

#include <oui/container.h>

#include <memory>

namespace olagarro::oui
{

class Row final : public Container
{
  public:
    void set_gap(Spacing gap);
    void set_centered(bool centered);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;

  private:
    Spacing m_gap{Spacing::none};
    bool m_centered{false};
};

class Column final : public Container
{
  public:
    void set_gap(Spacing gap);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;

  private:
    Spacing m_gap{Spacing::none};
};

[[nodiscard]] std::unique_ptr<Row> row();
Row& row(Container& parent);
[[nodiscard]] std::unique_ptr<Column> column();
Column& column(Container& parent);

} // namespace olagarro::oui
