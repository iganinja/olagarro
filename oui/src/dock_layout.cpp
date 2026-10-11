#include <oui/dock_layout.h>
#include <oui/painter.h>

#include <algorithm>

namespace olagarro::oui
{

Size DockLayout::measure(const Constraints& constraints) const
{
    return constraints.constrain({constraints.max_width, constraints.max_height});
}

void DockLayout::set_resizable(const Widget& child, int minimum)
{
    m_docks[&child].minimum = std::max(1, minimum);
}

int DockLayout::size_of(const Widget& child) const
{
    const auto found{m_docks.find(&child)};
    return found == m_docks.end() ? 0 : found->second.size;
}

void DockLayout::set_size(const Widget& child, int size)
{
    m_docks[&child].size = size;
    if(bounds().width > 0)
    {
        arrange(bounds());
    }
}

void DockLayout::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    m_dividers.clear();
    auto remaining{bounds};
    for(std::size_t index{}; index < child_count(); ++ index)
    {
        auto& child{child_at(index)};
        if(!child.is_visible())
        {
            continue;
        }
        const auto found{m_docks.find(&child)};
        const auto placement{found == m_docks.end() ? Placement{} : found->second};
        const auto measured{[&child, &remaining] { return child.measure({remaining.width, remaining.height}); }};
        switch(placement.dock)
        {
        case Dock::top:
        {
            const auto height{std::clamp(placement.size > 0 ? placement.size : measured().height, 0, remaining.height)};
            child.arrange({remaining.x, remaining.y, remaining.width, height});
            remaining.y += height;
            remaining.height -= height;
            break;
        }
        case Dock::bottom:
        {
            const auto height{std::clamp(placement.size > 0 ? placement.size : measured().height, 0, remaining.height)};
            child.arrange({remaining.x, remaining.y + remaining.height - height, remaining.width, height});
            remaining.height -= height;
            break;
        }
        case Dock::left:
        case Dock::right:
        {
            auto width{std::clamp(placement.size > 0 ? placement.size : measured().width, 0, remaining.width)};
            const auto left{placement.dock == Dock::left};
            const auto x{left ? remaining.x : remaining.x + remaining.width - width};
            // A resizable strip gives its inner column to the line, and keeps room on both sides of it.
            if(placement.minimum > 0 && width > placement.minimum)
            {
                const auto room{std::max(placement.minimum, remaining.width - placement.minimum)};
                width = std::min(width, room);
                const auto strip_x{left ? remaining.x : remaining.x + remaining.width - width};
                m_dividers.push_back({&child,
                                      {left ? strip_x + width - 1 : strip_x, remaining.y, 1, remaining.height},
                                      left ? remaining.x : remaining.x + remaining.width,
                                      room});
                child.arrange({left ? strip_x : strip_x + 1, remaining.y, width - 1, remaining.height});
            }
            else
            {
                child.arrange({x, remaining.y, width, remaining.height});
            }
            if(left)
            {
                remaining.x += width;
            }
            remaining.width -= width;
            break;
        }
        case Dock::fill:
            child.arrange(remaining);
            break;
        }
    }
}

void DockLayout::paint(Painter& painter, const Theme& theme) const
{
    Container::paint(painter, theme);
    for(const auto& divider : m_dividers)
    {
        painter.draw_divider(divider.line, theme);
    }
}

const DockLayout::Divider* DockLayout::divider_at(int x, int y) const
{
    const auto found{std::find_if(
        m_dividers.begin(), m_dividers.end(), [x, y](const Divider& divider)
        { return x == divider.line.x && y >= divider.line.y && y < divider.line.y + divider.line.height; })};
    return found == m_dividers.end() ? nullptr : &*found;
}

Widget* DockLayout::hit_test(int x, int y)
{
    // The lines come before what they divide, though an open popup of a child still comes first.
    if(auto* const target{Container::hit_test(x, y)}; target != nullptr && target->is_overlay())
    {
        return target;
    }
    if(divider_at(x, y) != nullptr)
    {
        return this;
    }
    return Container::hit_test(x, y);
}

bool DockLayout::handle_event(const Event& event)
{
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    if(mouse == nullptr)
    {
        return Container::handle_event(event);
    }
    if(mouse->pressed && !mouse->right)
    {
        if(const auto* const divider{divider_at(mouse->x, mouse->y)})
        {
            m_dragging = static_cast<std::size_t>(divider - m_dividers.data());
            return true;
        }
    }
    if(!m_dragging || *m_dragging >= m_dividers.size())
    {
        return Container::handle_event(event);
    }
    const auto divider{m_dividers[*m_dragging]};
    if(mouse->dragging || mouse->released)
    {
        auto& placement{m_docks[divider.child]};
        // The strip reaches from its edge to the pointer, the line included.
        const auto wanted{placement.dock == Dock::left ? mouse->x - divider.edge + 1 : divider.edge - mouse->x};
        placement.size = std::clamp(wanted, placement.minimum + 1, divider.room);
        arrange(bounds());
        if(mouse->released)
        {
            m_dragging.reset();
            m_resized.fire(*divider.child, placement.size);
        }
    }
    return true;
}

Pointer DockLayout::pointer_at(Point point) const
{
    // Its lines part side panels from what is beside them: they move left and right.
    return m_dragging || divider_at(point.x, point.y) != nullptr ? Pointer::resize_columns : Pointer::arrow;
}

std::unique_ptr<DockLayout> dock_layout()
{
    return std::make_unique<DockLayout>();
}

DockLayout& dock_layout(Container& parent)
{
    return parent.add(dock_layout());
}

} // namespace olagarro::oui
