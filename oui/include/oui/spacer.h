#pragma once

#include <oui/container.h>

#include <memory>

namespace olagarro::oui
{

class VSpacer final : public Widget
{
  public:
    explicit VSpacer(int height);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    int m_height;
};

[[nodiscard]] std::unique_ptr<VSpacer> vspacer(int height);
VSpacer& vspacer(int height, Container& parent);

class HSpacer final : public Widget
{
  public:
    explicit HSpacer(int width);
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    int m_width;
};

[[nodiscard]] std::unique_ptr<HSpacer> hspacer(int width);
HSpacer& hspacer(int width, Container& parent);

} // namespace olagarro::oui
