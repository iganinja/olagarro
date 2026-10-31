#include <oui/split_view.h>

#include <oui/painter.h>
#include <oui/text_metrics.h>
#include <oui/theme.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

namespace olagarro::oui
{

SplitView::SplitView(Orientation orientation) : m_orientation{orientation}
{
}

void SplitView::check_room() const
{
    if(child_count() >= 2)
    {
        throw std::logic_error{"A SplitView holds two widgets."};
    }
}

void SplitView::set_orientation(Orientation orientation)
{
    m_orientation = orientation;
}

SplitView::Orientation SplitView::orientation() const
{
    return m_orientation;
}

void SplitView::set_ratio(double ratio)
{
    m_ratio = std::clamp(ratio, 0.0, 1.0);
}

double SplitView::ratio() const
{
    return m_ratio;
}

void SplitView::move_divider(int cells)
{
    const auto line{divider()};
    if(!line)
    {
        return;
    }
    const auto horizontal{m_orientation == Orientation::horizontal};
    const auto start{horizontal ? bounds().x : bounds().y};
    const auto length{horizontal ? bounds().width : bounds().height};
    if(length > 2)
    {
        const auto moved{std::clamp(*line - start + cells, 1.0, length - 2)};
        set_ratio(moved / (length - 1));
        arrange(bounds());
    }
}

std::optional<double> SplitView::divider() const
{
    if(child_count() < 2 || !child_at(0).is_visible() || !child_at(1).is_visible())
    {
        return std::nullopt;
    }
    const auto horizontal{m_orientation == Orientation::horizontal};
    const auto start{horizontal ? bounds().x : bounds().y};
    const auto length{horizontal ? bounds().width : bounds().height};
    // Each side keeps at least one cell, and the line one more; it lies at the step nearest the ratio.
    const auto step{horizontal ? text_metrics().column_step() : text_metrics().row_step()};
    const auto first{std::clamp(snap(m_ratio * (length - 1) + step / 2, step), 1.0, std::max(1.0, length - 2))};
    return start + first;
}

Size SplitView::measure(const Constraints& constraints) const
{
    return constraints.constrain({constraints.max_width, constraints.max_height});
}

void SplitView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto line{divider()};
    for(std::size_t index{}; index < child_count(); ++ index)
    {
        auto& child{child_at(index)};
        if(!child.is_visible())
        {
            continue;
        }
        if(!line)
        {
            child.arrange(bounds);
            continue;
        }
        if(m_orientation == Orientation::horizontal)
        {
            child.arrange(index == 0 ? Rect{bounds.x, bounds.y, *line - bounds.x, bounds.height}
                                     : Rect{*line + 1, bounds.y, bounds.x + bounds.width - *line - 1, bounds.height});
        }
        else
        {
            child.arrange(index == 0 ? Rect{bounds.x, bounds.y, bounds.width, *line - bounds.y}
                                     : Rect{bounds.x, *line + 1, bounds.width, bounds.y + bounds.height - *line - 1});
        }
    }
}

void SplitView::paint(Painter& painter, const Theme& theme) const
{
    Container::paint(painter, theme);
    const auto line{divider()};
    if(!line)
    {
        return;
    }
    painter.draw_divider(m_orientation == Orientation::horizontal ? Rect{*line, bounds().y, 1, bounds().height}
                                                                  : Rect{bounds().x, *line, bounds().width, 1},
                         theme);
}

bool SplitView::handle_event(const Event& event)
{
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    const auto line{divider()};
    if(mouse == nullptr || !line)
    {
        return false;
    }
    const auto horizontal{m_orientation == Orientation::horizontal};
    const auto position{horizontal ? mouse->x : mouse->y};
    if(mouse->pressed && !mouse->right && whole(position - *line) == 0)
    {
        m_dragging = true;
        m_grab = position - *line;
        return true;
    }
    if(!m_dragging)
    {
        return false;
    }
    if(mouse->dragging || mouse->released)
    {
        const auto start{horizontal ? bounds().x : bounds().y};
        const auto length{horizontal ? bounds().width : bounds().height};
        if(length > 1)
        {
            // To the step, which divider() keeps it at.
            set_ratio((position - m_grab - start) / (length - 1));
        }
    }
    if(mouse->released)
    {
        m_dragging = false;
    }
    return true;
}

Pointer SplitView::pointer_at(Point point) const
{
    const auto line{divider()};
    const auto horizontal{m_orientation == Orientation::horizontal};
    if(m_dragging || (line && whole((horizontal ? point.x : point.y) - *line) == 0))
    {
        return horizontal ? Pointer::resize_columns : Pointer::resize_rows;
    }
    return Pointer::arrow;
}

std::unique_ptr<SplitView> split_view(SplitView::Orientation orientation)
{
    return std::make_unique<SplitView>(orientation);
}

SplitView& split_view(Container& parent, SplitView::Orientation orientation)
{
    return parent.add(split_view(orientation));
}

} // namespace olagarro::oui
