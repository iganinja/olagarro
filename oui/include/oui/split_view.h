#pragma once

#include <oui/container.h>

#include <memory>
#include <optional>

namespace olagarro::oui
{

// Two widgets side by side, or one above the other, with a line between them that the mouse drags to share
// the space differently. When either is hidden or missing, the other takes the whole space and the line goes.
// Split views inside split views divide the space as finely as wanted.
class SplitView final : public Container
{
  public:
    enum class Orientation
    {
        // Side by side, with an upright line between them.
        horizontal,
        vertical
    };

    explicit SplitView(Orientation orientation = Orientation::horizontal);

    // The first and second widgets are the first two added; more are an error.
    template <typename Child> Child& add(std::unique_ptr<Child>&& child)
    {
        check_room();
        return Container::add(std::move(child));
    }
    void set_orientation(Orientation orientation);
    [[nodiscard]] Orientation orientation() const;
    // The first widget's share of the space, between 0 and 1; half unless set.
    void set_ratio(double ratio);
    [[nodiscard]] double ratio() const;
    // Moves the line by whole cells, right or down for a positive count, as a drag with the mouse would; each
    // side keeps at least one cell.
    void move_divider(int cells);

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Pointer pointer_at(Point point) const override;

  private:
    void check_room() const;
    // Where the line is, as a column or a row, when both widgets show.
    [[nodiscard]] std::optional<int> divider() const;

    Orientation m_orientation;
    double m_ratio{0.5};
    bool m_dragging{false};
};

[[nodiscard]] std::unique_ptr<SplitView>
split_view(SplitView::Orientation orientation = SplitView::Orientation::horizontal);
SplitView& split_view(Container& parent, SplitView::Orientation orientation = SplitView::Orientation::horizontal);

} // namespace olagarro::oui
