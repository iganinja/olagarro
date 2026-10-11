#pragma once

#include <oui/container.h>
#include <oui/scrollbar.h>

#include <memory>

namespace olagarro::oui
{

class ScrollView final : public Container
{
  public:
    [[nodiscard]] bool is_focusable() const override;
    [[nodiscard]] int scroll_offset() const;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    [[nodiscard]] int max_scroll_offset() const;

    int m_scroll_offset{};
    // The scrollbar, pressed and dragged.
    ScrollbarDrag m_scrollbar;
    int m_content_height{};
    int m_viewport_height{};
};

[[nodiscard]] std::unique_ptr<ScrollView> scroll_view();
ScrollView& scroll_view(Container& parent);

} // namespace olagarro::oui
