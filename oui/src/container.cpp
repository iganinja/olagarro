#include <oui/container.h>
#include <oui/trace.h>

#include <stdexcept>

namespace olagarro::oui
{

namespace
{

// Overlays such as an open combo box list spill over the widgets around them, so they paint after all
// of them and take clicks first, however deep in the tree they sit.
void paint_descendant_overlays(const Container& container, Painter& painter, const Theme& theme)
{
    for(std::size_t index{}; index < container.child_count(); ++ index)
    {
        const Widget& child{container.child_at(index)};
        if(!child.is_visible())
        {
            continue;
        }
        if(child.is_overlay())
        {
            const TraceScope span{child};
            child.paint(painter, theme);
            continue;
        }
        if(const auto* const nested{dynamic_cast<const Container*>(&child)})
        {
            paint_descendant_overlays(*nested, painter, theme);
        }
    }
}

Widget* overlay_at(Container& container, double x, double y)
{
    for(std::size_t index{container.child_count()}; index > 0; -- index)
    {
        Widget& child{container.child_at(index - 1)};
        if(!child.is_visible())
        {
            continue;
        }
        if(child.is_overlay())
        {
            if(auto* const result{child.hit_test(x, y)})
            {
                return result;
            }
        }
        if(auto* const nested{dynamic_cast<Container*>(&child)})
        {
            if(auto* const result{overlay_at(*nested, x, y)})
            {
                return result;
            }
        }
    }
    return nullptr;
}

} // namespace

std::size_t Container::child_count() const
{
    return m_children.size();
}
std::unique_ptr<Widget> Container::remove_at(std::size_t index)
{
    std::unique_ptr<Widget> result{std::move(m_children.at(index))};
    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));
    result->set_parent(nullptr);
    return result;
}
std::size_t Container::index_of(const Widget& child) const
{
    for(std::size_t index{}; index < m_children.size(); ++ index)
    {
        if(m_children[index].get() == &child)
        {
            return index;
        }
    }
    throw std::logic_error{"Not a child of this container."};
}
std::unique_ptr<Widget> Container::remove(const Widget& child)
{
    return remove_at(index_of(child));
}
std::unique_ptr<Widget> Container::replace(const Widget& child, std::unique_ptr<Widget> replacement)
{
    const auto index{index_of(child)};
    auto previous{std::move(m_children[index])};
    previous->set_parent(nullptr);
    replace_child(index, std::move(replacement));
    return previous;
}
Widget& Container::child_at(std::size_t index)
{
    return *m_children.at(index);
}
const Widget& Container::child_at(std::size_t index) const
{
    return *m_children.at(index);
}
void Container::paint(Painter& painter, const Theme& theme) const
{
    for(const auto& child : m_children)
    {
        if(child->is_visible() && !child->is_overlay())
        {
            painter.push_clip(child->paint_bounds());
            {
                const TraceScope span{*child};
                child->paint(painter, theme);
            }
            painter.pop_clip();
        }
    }
    paint_descendant_overlays(*this, painter, theme);
}
void Container::collect_focusable(std::vector<Widget*>& widgets)
{
    Widget::collect_focusable(widgets);
    for(const auto& child : m_children)
    {
        if(child->is_visible())
        {
            child->collect_focusable(widgets);
        }
    }
}
Widget* Container::find_by_id(std::string_view id)
{
    if(auto* const self{Widget::find_by_id(id)})
    {
        return self;
    }
    for(const auto& child : m_children)
    {
        if(auto* const result{child->find_by_id(id)})
        {
            return result;
        }
    }
    return nullptr;
}
bool Container::contains(const Widget* const target) const
{
    if(Widget::contains(target))
    {
        return true;
    }
    for(const auto& child : m_children)
    {
        if(child->contains(target))
        {
            return true;
        }
    }
    return false;
}
Widget* Container::hit_test(double x, double y)
{
    // An overlay can reach outside this container, so it is asked before the bounds are.
    if(auto* const overlay{overlay_at(*this, x, y)})
    {
        return overlay;
    }
    if(Widget::hit_test(x, y) == nullptr)
    {
        return nullptr;
    }
    for(auto iterator{m_children.rbegin()}; iterator != m_children.rend(); ++ iterator)
    {
        if((*iterator)->is_visible() && !(*iterator)->is_overlay())
        {
            if(auto* const result{(*iterator)->hit_test(x, y)})
            {
                return result;
            }
        }
    }
    return this;
}
void Container::dismiss_overlays_at(double x, double y)
{
    for(const auto& child : m_children)
    {
        if(child->is_visible())
        {
            child->dismiss_overlays_at(x, y);
        }
    }
}
Widget* Container::keyboard_target()
{
    for(auto iterator{m_children.rbegin()}; iterator != m_children.rend(); ++ iterator)
    {
        if((*iterator)->is_visible() && (*iterator)->is_overlay())
        {
            if(auto* const target{(*iterator)->keyboard_target()})
            {
                return target;
            }
        }
    }
    for(auto iterator{m_children.rbegin()}; iterator != m_children.rend(); ++ iterator)
    {
        if((*iterator)->is_visible() && !(*iterator)->is_overlay())
        {
            if(auto* const target{(*iterator)->keyboard_target()})
            {
                return target;
            }
        }
    }
    return nullptr;
}
const std::vector<std::unique_ptr<Widget>>& Container::children() const
{
    return m_children;
}
std::vector<std::unique_ptr<Widget>>& Container::children_mutable()
{
    return m_children;
}

} // namespace olagarro::oui
