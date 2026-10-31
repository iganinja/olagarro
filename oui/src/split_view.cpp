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
    const auto thickness{divider_thickness()};
    if(length > thickness + 1)
    {
        const auto moved{std::clamp(*line - start + cells, 1.0, length - thickness - 1)};
        set_ratio(moved / (length - thickness));
        arrange(bounds());
    }
}

double SplitView::divider_thickness() const
{
    // A cell in a terminal, 6 pixels in a window.
    const auto line{Length::pixels(6, 1)};
    return m_orientation == Orientation::horizontal ? length_across(line) : length_down(line);
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
    const auto thickness{divider_thickness()};
    // Each side keeps at least one cell, and the line its thickness; it lies at the step nearest the ratio.
    const auto step{horizontal ? text_metrics().column_step() : text_metrics().row_step()};
    const auto first{
        std::clamp(snap(m_ratio * (length - thickness) + step / 2, step), 1.0, std::max(1.0, length - thickness - 1))};
    return start + first;
}

bool SplitView::on_divider(double position) const
{
    // Within half a cell of the line's middle, which in a terminal is its cell and in a window more than its pixels:
    // a thin line is as easy to take as a cell is.
    const auto line{divider()};
    if(!line)
    {
        return false;
    }
    const auto middle{*line + divider_thickness() / 2};
    return position >= middle - 0.5 && position < middle + 0.5;
}

Widget* SplitView::hit_test(double x, double y)
{
    auto* const result{Container::hit_test(x, y)};
    if(result == nullptr || result == this || !on_divider(m_orientation == Orientation::horizontal ? x : y))
    {
        return result;
    }
    // Near the line the pointer is on it, though a side's pixels lie there, but for an overlay over it.
    for(const auto* widget{result}; widget != nullptr && widget != this; widget = widget->parent())
    {
        if(widget->is_overlay())
        {
            return result;
        }
    }
    return this;
}

Size SplitView::measure(const Constraints& constraints) const
{
    return constraints.constrain({constraints.max_width, constraints.max_height});
}

void SplitView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto line{divider()};
    const auto thickness{divider_thickness()};
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
                                     : Rect{*line + thickness, bounds.y, bounds.x + bounds.width - *line - thickness,
                                            bounds.height});
        }
        else
        {
            child.arrange(index == 0 ? Rect{bounds.x, bounds.y, bounds.width, *line - bounds.y}
                                     : Rect{bounds.x, *line + thickness, bounds.width,
                                            bounds.y + bounds.height - *line - thickness});
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
    const auto thickness{divider_thickness()};
    painter.draw_divider(m_orientation == Orientation::horizontal ? Rect{*line, bounds().y, thickness, bounds().height}
                                                                  : Rect{bounds().x, *line, bounds().width, thickness},
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
    if(mouse->pressed && !mouse->right && on_divider(position))
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
        const auto thickness{divider_thickness()};
        if(length > thickness)
        {
            // To the step, which divider() keeps it at.
            set_ratio((position - m_grab - start) / (length - thickness));
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
    const auto horizontal{m_orientation == Orientation::horizontal};
    if(m_dragging || on_divider(horizontal ? point.x : point.y))
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
