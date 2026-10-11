#include <oui/scroll_view.h>

#include <algorithm>
#include <limits>

namespace olagarro::oui
{

bool ScrollView::is_focusable() const
{
    return true;
}
int ScrollView::scroll_offset() const
{
    return m_scroll_offset;
}

bool ScrollView::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(m_content_height > m_viewport_height)
        {
            if(const auto to{m_scrollbar.handle(
                   *mouse, {bounds().x + bounds().width - 1, bounds().y, 1, bounds().height}, bounds().height,
                   m_content_height, m_scroll_offset, max_scroll_offset(), std::max(1, m_viewport_height - 1))})
            {
                m_scroll_offset = *to;
                return true;
            }
        }
        if(mouse->wheel_delta == 0)
        {
            return false;
        }
        m_scroll_offset = std::clamp(m_scroll_offset - mouse->wheel_delta, 0, max_scroll_offset());
        return true;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr)
    {
        return false;
    }
    if(key->key == Key::up)
    {
        m_scroll_offset = std::max(0, m_scroll_offset - 1);
        return true;
    }
    if(key->key == Key::down)
    {
        m_scroll_offset = std::min(max_scroll_offset(), m_scroll_offset + 1);
        return true;
    }
    return false;
}

Size ScrollView::measure(const Constraints& constraints) const
{
    return children().empty() ? Size{} : constraints.constrain(children().front()->measure(constraints));
}

void ScrollView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(children().empty())
    {
        return;
    }
    const auto content{children().front()->measure({bounds.width, std::numeric_limits<int>::max()})};
    m_content_height = content.height;
    m_viewport_height = bounds.height;
    m_scroll_offset = std::min(m_scroll_offset, max_scroll_offset());
    children().front()->arrange({bounds.x, bounds.y - m_scroll_offset, bounds.width, content.height});
}

void ScrollView::paint(Painter& painter, const Theme& theme) const
{
    Container::paint(painter, theme);
    if(m_content_height <= m_viewport_height || bounds().width <= 0 || bounds().height <= 0)
    {
        return;
    }
    const auto track_x{bounds().x + bounds().width - 1};
    const auto thumb{
        scrollbar_thumb(bounds().height, bounds().height, m_content_height, m_scroll_offset, max_scroll_offset())};
    const auto thumb_y{bounds().y + thumb.start};
    const auto thumb_height{thumb.length};
    painter.draw_scrollbar({track_x, bounds().y, 1, bounds().height},
                           {thumb_y, thumb_height, theme.panel_background, theme.selection_foreground}, theme);
}

int ScrollView::max_scroll_offset() const
{
    return std::max(0, m_content_height - m_viewport_height);
}

std::unique_ptr<ScrollView> scroll_view()
{
    return std::make_unique<ScrollView>();
}
ScrollView& scroll_view(Container& parent)
{
    return parent.add(scroll_view());
}

} // namespace olagarro::oui
