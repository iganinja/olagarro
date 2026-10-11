#include <oui/layout.h>

#include <algorithm>

namespace olagarro::oui
{

void Row::set_gap(Spacing gap)
{
    m_gap = gap;
}
void Row::set_centered(bool centered)
{
    m_centered = centered;
}

Size Row::measure(const Constraints& constraints) const
{
    Size size{};
    const auto gap{spacing_cells(m_gap)};
    auto has_child{false};
    for(const auto& child : children())
    {
        // A hidden widget takes no room, nor a gap.
        if(!child->is_visible())
        {
            continue;
        }
        const auto child_size{child->measure(constraints)};
        size.width += child_size.width + (has_child ? gap : 0);
        size.height = std::max(size.height, child_size.height);
        has_child = true;
    }
    return constraints.constrain(size);
}

void Row::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    auto x{bounds.x};
    const auto gap{spacing_cells(m_gap)};
    if(m_centered)
    {
        x += std::max(0, (bounds.width - measure({bounds.width, bounds.height}).width) / 2);
    }
    for(const auto& child : children())
    {
        if(!child->is_visible())
        {
            child->arrange({x, bounds.y, 0, 0});
            continue;
        }
        const auto child_size{child->measure({bounds.width, bounds.height})};
        child->arrange({x, bounds.y, child_size.width, bounds.height});
        x += child_size.width + gap;
    }
}

void Column::set_gap(Spacing gap)
{
    m_gap = gap;
}

Size Column::measure(const Constraints& constraints) const
{
    Size size{};
    const auto gap{spacing_cells(m_gap)};
    auto has_child{false};
    for(const auto& child : children())
    {
        if(!child->is_visible())
        {
            continue;
        }
        const auto child_size{child->measure(constraints)};
        size.width = std::max(size.width, child_size.width);
        size.height += child_size.height + (has_child ? gap : 0);
        has_child = true;
    }
    return constraints.constrain(size);
}

void Column::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    auto y{bounds.y};
    const auto gap{spacing_cells(m_gap)};
    for(const auto& child : children())
    {
        if(!child->is_visible())
        {
            child->arrange({bounds.x, y, 0, 0});
            continue;
        }
        const auto remaining_height{std::max(0, bounds.height - (y - bounds.y))};
        const auto child_size{child->measure({bounds.width, remaining_height})};
        child->arrange({bounds.x, y, bounds.width, child_size.height});
        y += child_size.height + gap;
    }
}

std::unique_ptr<Row> row()
{
    return std::make_unique<Row>();
}
Row& row(Container& parent)
{
    return parent.add(row());
}
std::unique_ptr<Column> column()
{
    return std::make_unique<Column>();
}
Column& column(Container& parent)
{
    return parent.add(column());
}

} // namespace olagarro::oui
