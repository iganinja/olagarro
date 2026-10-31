#include <oui/panel.h>

#include <algorithm>

namespace olagarro::oui
{

void Panel::set_padding(Insets padding)
{
    m_padding = padding;
}

Size Panel::measure(const Constraints& constraints) const
{
    if(children().empty())
    {
        return {};
    }
    const auto child{children().front()->measure(constraints)};
    return constraints.constrain(
        {child.width + m_padding.left + m_padding.right, child.height + m_padding.top + m_padding.bottom});
}

void Panel::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(!children().empty())
    {
        children().front()->arrange({bounds.x + m_padding.left, bounds.y + m_padding.top,
                                     std::max(0.0, bounds.width - m_padding.left - m_padding.right),
                                     std::max(0.0, bounds.height - m_padding.top - m_padding.bottom)});
    }
}

void Panel::paint(Painter& painter, const Theme& theme) const
{
    painter.fill_rect(bounds(), theme.panel_background);
    Container::paint(painter, theme);
}

std::unique_ptr<Panel> panel()
{
    return std::make_unique<Panel>();
}
Panel& panel(Container& parent)
{
    return parent.add(panel());
}

} // namespace olagarro::oui
