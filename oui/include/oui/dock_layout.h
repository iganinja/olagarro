#pragma once

#include <oui/container.h>

#include <signalslot/signalslot.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace olagarro::oui
{

enum class Dock
{
    top,
    bottom,
    left,
    right,
    fill
};

// Lays children against the edges in the order they were added: each takes a strip along its edge of the
// space the earlier ones left, and fill children share whatever remains. A hidden child takes no space.
// A menu bar on top, a status bar at the bottom, a side panel on the left and the document filling the rest
// is four calls.
class DockLayout final : public Container
{
  public:
    // `size` is the strip's height for top and bottom, its width for left and right; 0 uses the child's own
    // measured size.
    template <typename Child> Child& add(Dock dock, std::unique_ptr<Child>&& child, int size = 0)
    {
        auto& added{Container::add(std::move(child))};
        m_docks[&added] = {dock, size};
        return added;
    }

    // Gives a left or right strip a line along its inner edge that the mouse drags to make it wider or narrower;
    // the line is part of the strip's size. A strip keeps at least `minimum` cells, and leaves as many to the rest.
    void set_resizable(const Widget& child, int minimum = 8);
    // The strip's size, line included, as set or as last dragged.
    [[nodiscard]] int size_of(const Widget& child) const;
    void set_size(const Widget& child, int size);
    // Fires with the strip and its new size when a drag ends.
    template <typename Callback> olagarro::signal_slot::Connection on_resized(Callback&& callback)
    {
        return m_resized.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    [[nodiscard]] Widget* hit_test(double x, double y) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Pointer pointer_at(Point point) const override;

  private:
    struct Placement
    {
        Dock dock{Dock::fill};
        int size{};
        // 0 when the strip is not resizable.
        int minimum{};
    };
    // A resizable strip's line as last arranged: where it is, and how far the strip may grow from its edge.
    struct Divider
    {
        const Widget* child{};
        Rect line;
        double edge{};
        double room{};
    };
    [[nodiscard]] const Divider* divider_at(double x, double y) const;

    std::map<const Widget*, Placement> m_docks;
    std::vector<Divider> m_dividers;
    std::optional<std::size_t> m_dragging;
    olagarro::signal_slot::Signal<const Widget&, int> m_resized;
};

[[nodiscard]] std::unique_ptr<DockLayout> dock_layout();
DockLayout& dock_layout(Container& parent);

} // namespace olagarro::oui
