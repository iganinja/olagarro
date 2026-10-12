#pragma once

#include <oui/container.h>

#include <memory>
#include <string>
#include <string_view>

namespace olagarro::oui
{

// How far something has gone, as a bar that fills from the left, with a text after it, as "4 of 8".
class ProgressBar final : public Widget
{
  public:
    // From 0, nothing done, to 1, all of it; kept within them.
    void set_value(double value);
    [[nodiscard]] double value() const;
    // What it says after the bar; nothing when empty.
    void set_text(std::string_view text);
    [[nodiscard]] std::string_view text() const;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    double m_value{0.0};
    std::string m_text;
};

[[nodiscard]] std::unique_ptr<ProgressBar> progress_bar();
ProgressBar& progress_bar(Container& parent);

} // namespace olagarro::oui
