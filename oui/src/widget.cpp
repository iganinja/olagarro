#include <oui/widget.h>

#include <oui/container.h>

namespace olagarro::oui
{

void Widget::arrange(const Rect& bounds)
{
    m_bounds = bounds;
}
void Widget::paint(Painter&, const Theme&) const
{
}
bool Widget::is_focusable() const
{
    return false;
}
bool Widget::handle_event(const Event&)
{
    return false;
}
bool Widget::claims_key(const Event&) const
{
    return false;
}
std::optional<std::string> Widget::tooltip_at(Point) const
{
    return m_tooltip.empty() ? std::nullopt : std::optional<std::string>{m_tooltip};
}
Pointer Widget::pointer_at(Point) const
{
    return Pointer::arrow;
}
void Widget::set_tooltip(std::string text)
{
    m_tooltip = std::move(text);
}
void Widget::collect_focusable(std::vector<Widget*>& widgets)
{
    if(is_focusable() && is_enabled())
    {
        widgets.push_back(this);
    }
}
bool Widget::is_focused() const
{
    return m_is_focused;
}
bool Widget::is_visible() const
{
    return m_is_visible;
}
void Widget::set_visible(bool visible)
{
    m_is_visible = visible;
}
bool Widget::is_enabled() const
{
    return m_is_enabled && (m_parent == nullptr || m_parent->is_enabled());
}
void Widget::set_enabled(bool enabled)
{
    m_is_enabled = enabled;
}
void Widget::set_key_handler(std::function<bool(const Event&)> handler)
{
    m_key_handler = std::move(handler);
}
bool Widget::key_handler_keeps(const Event& event) const
{
    return m_key_handler && (std::holds_alternative<KeyEvent>(event) || std::holds_alternative<TextEvent>(event)) &&
           m_key_handler(event);
}
void Widget::set_id(std::string id)
{
    m_id = std::move(id);
}
std::string_view Widget::id() const
{
    return m_id;
}
Widget* Widget::find_by_id(std::string_view id)
{
    return m_id == id ? this : nullptr;
}
bool Widget::contains(const Widget* const target) const
{
    return this == target;
}
Widget* Widget::hit_test(int x, int y)
{
    // A disabled widget is not there for the pointer: what is under it is.
    if(!is_enabled())
    {
        return nullptr;
    }
    return x >= m_bounds.x && x < m_bounds.x + m_bounds.width && y >= m_bounds.y && y < m_bounds.y + m_bounds.height
               ? this
               : nullptr;
}
bool Widget::is_selected_child(const Widget&) const
{
    return false;
}
const Rect& Widget::bounds() const
{
    return m_bounds;
}
Rect Widget::paint_bounds() const
{
    return m_bounds;
}
bool Widget::is_overlay() const
{
    return false;
}
void Widget::dismiss_overlays_at(int, int)
{
}
std::weak_ptr<void> Widget::lifetime() const
{
    return m_lifetime;
}
void Widget::delegate_typing_to(Widget& target)
{
    m_typing_delegate = &target;
    m_typing_delegate_lifetime = target.m_lifetime;
}

Widget* Widget::keyboard_target()
{
    return nullptr;
}
const Container* Widget::parent() const
{
    return m_parent;
}
Container* Widget::parent()
{
    return m_parent;
}
void Widget::set_parent(Container* const parent)
{
    m_parent = parent;
}
void Widget::set_focused(bool focused)
{
    m_is_focused = focused;
}

} // namespace olagarro::oui
