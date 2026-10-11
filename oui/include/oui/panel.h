#pragma once

#include <oui/container.h>

#include <memory>

namespace olagarro::oui
{

class Panel final : public Container
{
  public:
    void set_padding(Insets padding);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    Insets m_padding{};
};

[[nodiscard]] std::unique_ptr<Panel> panel();
Panel& panel(Container& parent);

} // namespace olagarro::oui
