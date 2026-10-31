#include <oui/window.h>
#include <oui/button.h>
#include <oui/text_metrics.h>

#include <algorithm>
#include <cmath>

namespace olagarro::oui
{

namespace
{

// The visible button marked default in `widget` or under it, if any.
Button* default_button(Widget& widget)
{
    if(!widget.is_visible() || !widget.is_enabled())
    {
        return nullptr;
    }
    if(auto* const button{dynamic_cast<Button*>(&widget)}; button != nullptr && button->is_default())
    {
        return button;
    }
    if(auto* const container{dynamic_cast<Container*>(&widget)})
    {
        for(std::size_t index{}; index < container->child_count(); ++ index)
        {
            if(auto* const found{default_button(container->child_at(index))})
            {
                return found;
            }
        }
    }
    return nullptr;
}

} // namespace

namespace
{

void clear_rect(Painter& painter, const Rect& rect, Color background)
{
    // Only the background shows: the cells hold spaces.
    if(rect.width <= 0 || rect.height <= 0)
    {
        return;
    }

    const std::string spaces(static_cast<std::size_t>(rect.width), ' ');
    const TextStyle style{background, background};
    for(auto y{rect.y}; y < rect.y + rect.height; ++ y)
    {
        painter.draw_text({rect.x, y}, spaces, style);
    }
}

} // namespace

Window::Window(std::string_view title) : m_title{title}
{
}

std::string_view Window::title() const
{
    return m_title;
}
void Window::set_title(std::string title)
{
    m_title = std::move(title);
}
bool Window::is_closed() const
{
    return m_is_closed;
}
void Window::set_close_on_escape(bool close)
{
    m_close_on_escape = close;
}
bool Window::closes_on_escape() const
{
    return m_close_on_escape;
}
void Window::set_close_on_enter(bool close)
{
    m_close_on_enter = close;
}
void Window::set_border(WindowBorder border)
{
    m_border = border;
}
WindowBorder Window::border() const
{
    return m_border;
}
void Window::set_background(ColorRef background)
{
    m_background = background;
}
void Window::set_maximum_size(Size size)
{
    m_maximum_size = size;
}
Size Window::preferred_size() const
{
    return m_preferred_size;
}

void Window::set_preferred_size(Size size)
{
    m_preferred_size = size;
}
void Window::set_height_share(double share)
{
    m_height_share = share;
}

void Window::set_minimum_size(Size size)
{
    m_minimum_size = size;
}
void Window::set_resizable(bool resizable)
{
    m_resizable = resizable;
}
void Window::set_placement_key(std::string key)
{
    m_placement_key = std::move(key);
}
const std::string& Window::placement_key() const
{
    return m_placement_key;
}
WindowPlacement Window::placement() const
{
    WindowPlacement result;
    if(m_moved)
    {
        result.position = Point{bounds().x, bounds().y};
    }
    result.size = m_user_size;
    return result;
}
void Window::restore_placement(const WindowPlacement& placement)
{
    if(placement.size && m_resizable)
    {
        m_user_size = placement.size;
    }
    if(placement.position)
    {
        m_restored_position = placement.position;
        m_moved = true;
    }
}

void Window::start_placing()
{
    m_placing = Placing{m_drag_offset_x, m_drag_offset_y, m_user_size, m_moved};
}

bool Window::is_placing() const
{
    return m_placing.has_value();
}

bool Window::is_resizable() const
{
    return m_resizable;
}

bool Window::place(const KeyEvent& key)
{
    if(!m_placing)
    {
        return false;
    }
    if(key.key == Key::enter)
    {
        m_placing.reset();
        return false;
    }
    if(key.key == Key::escape)
    {
        m_drag_offset_x = m_placing->offset_x;
        m_drag_offset_y = m_placing->offset_y;
        m_user_size = m_placing->user_size;
        m_moved = m_placing->moved;
        m_placing.reset();
        return false;
    }
    const auto columns{key.key == Key::left ? -1 : key.key == Key::right ? 1 : 0};
    const auto rows{key.key == Key::up ? -1 : key.key == Key::down ? 1 : 0};
    if(columns == 0 && rows == 0)
    {
        return true;
    }
    if(key.control)
    {
        if(m_resizable)
        {
            resize_to({bounds().width + columns, bounds().height + rows});
            // Laid out again at once, so the next key, even in the same frame, starts from here.
            arrange(m_area);
        }
        return true;
    }
    // Within the screen, as arrange keeps it: an offset past the edge would only have to be undone.
    const auto left{bounds().x + columns};
    const auto top{bounds().y + rows};
    if(left >= m_area.x && left + bounds().width <= m_area.x + m_area.width)
    {
        m_drag_offset_x += columns;
    }
    if(top >= m_area.y && top + bounds().height <= m_area.y + m_area.height)
    {
        m_drag_offset_y += rows;
    }
    m_moved = true;
    arrange(m_area);
    return true;
}

void Window::close(DialogResult result)
{
    if(m_is_closed)
    {
        return;
    }
    m_is_closed = true;
    m_closed.fire(result);
}

void Window::reopen()
{
    m_is_closed = false;
    m_close_pressed = false;
}

Size Window::measure(const Constraints& constraints) const
{
    if(m_border == WindowBorder::none)
    {
        return children().empty() ? Size{} : children().front()->measure(constraints);
    }
    if(m_user_size)
    {
        return constraints.constrain(
            {std::max(m_user_size->width, m_minimum_size.width), std::max(m_user_size->height, m_minimum_size.height)});
    }
    const Constraints modal_constraints{std::min(constraints.max_width, m_maximum_size.width),
                                        std::min(constraints.max_height, m_maximum_size.height)};
    const auto child{children().empty() ? Size{0, 3} : children().front()->measure(modal_constraints)};
    const auto height{m_height_share
                          ? snap_y(modal_constraints.max_height * *m_height_share)
                          : std::max(child.height + text_metrics().window_top_rows() + 1, m_preferred_size.height)};
    const auto measured{modal_constraints.constrain(
        {std::max({static_cast<double>(m_title.size()) + 6, child.width + 2 * text_metrics().window_side_columns(),
                   m_preferred_size.width}),
         height})};
    return constraints.constrain(
        {std::max(measured.width, m_minimum_size.width), std::max(measured.height, m_minimum_size.height)});
}

void Window::arrange(const Rect& bounds)
{
    if(m_border == WindowBorder::none)
    {
        Widget::arrange(bounds);
        if(!children().empty())
        {
            children().front()->arrange(bounds);
        }
        return;
    }
    m_area = bounds;
    const auto size{measure({bounds.width, bounds.height})};
    const Rect centered{bounds.x + std::max(0.0, snap_x((bounds.width - size.width) / 2)),
                        bounds.y + std::max(0.0, snap_y((bounds.height - size.height) / 2)), size.width, size.height};
    if(m_restored_position)
    {
        m_drag_offset_x = m_restored_position->x - centered.x;
        m_drag_offset_y = m_restored_position->y - centered.y;
        m_restored_position.reset();
    }
    const Rect moved{
        std::clamp(centered.x + m_drag_offset_x, bounds.x, bounds.x + std::max(0.0, bounds.width - centered.width)),
        std::clamp(centered.y + m_drag_offset_y, bounds.y, bounds.y + std::max(0.0, bounds.height - centered.height)),
        centered.width, centered.height};
    Widget::arrange(moved);
    const auto top{text_metrics().window_top_rows()};
    const auto side{text_metrics().window_side_columns()};
    if(!children().empty())
    {
        children().front()->arrange({moved.x + side, moved.y + top, std::max(0.0, moved.width - 2 * side),
                                     std::max(0.0, moved.height - top - 1)});
    }
}

void Window::paint(Painter& painter, const Theme& theme) const
{
    const auto bg{resolve(m_background, theme, theme.dialog_background)};
    if(m_border == WindowBorder::none)
    {
        clear_rect(painter, bounds(), bg);
    }
    else
    {
        WindowLook look;
        look.title = m_title;
        look.double_border = m_border == WindowBorder::double_;
        look.resizable = m_resizable;
        look.placing = m_placing.has_value();
        look.closable = has_close_button();
        look.close_pressed = m_close_pressed;
        look.background = bg;
        painter.draw_window(bounds(), look, theme);
    }
    Container::paint(painter, theme);
}

bool Window::has_close_button() const
{
    return m_border != WindowBorder::none && m_close_on_escape && text_metrics().window_close_button();
}
bool Window::handle_event(const Event& event)
{
    if(const auto* const mouse_event{std::get_if<MouseEvent>(&event)})
    {
        // The pointer by the cell it is in, from the window's top left and from its last column and row: in a window
        // it may be anywhere in its cell, and the window may start between cells.
        const auto column{whole(mouse_event->x - bounds().x)};
        const auto row{whole(mouse_event->y - bounds().y)};
        const auto from_right{whole(mouse_event->x - (bounds().x + bounds().width - 1))};
        const auto from_bottom{whole(mouse_event->y - (bounds().y + bounds().height - 1))};
        if(m_resizable && mouse_event->pressed && !mouse_event->right)
        {
            const auto on_right{from_right == 0 && row > 0 && from_bottom <= 0};
            const auto on_bottom{from_bottom == 0 && column > 0 && from_right <= 0};
            if(on_right || on_bottom)
            {
                m_resizing_width = on_right;
                m_resizing_height = on_bottom;
                m_drag_start_x = mouse_event->x;
                m_drag_start_y = mouse_event->y;
                m_resize_start_size = {bounds().width, bounds().height};
                return true;
            }
        }
        if(mouse_event->dragging && (m_resizing_width || m_resizing_height))
        {
            resize_to({m_resizing_width ? m_resize_start_size.width + mouse_event->x - m_drag_start_x
                                        : m_resize_start_size.width,
                       m_resizing_height ? m_resize_start_size.height + mouse_event->y - m_drag_start_y
                                         : m_resize_start_size.height});
            return true;
        }
        if(mouse_event->released && (m_resizing_width || m_resizing_height))
        {
            m_resizing_width = false;
            m_resizing_height = false;
            return true;
        }
        // The close button: pressed and let go on it, the window closes as Escape closes it.
        const auto on_close{has_close_button() && row == 0 && from_right >= -2 && from_right <= 0};
        if(mouse_event->pressed && on_close)
        {
            m_close_pressed = true;
            return true;
        }
        if(m_close_pressed && (mouse_event->dragging || mouse_event->released))
        {
            if(mouse_event->released)
            {
                m_close_pressed = false;
                if(on_close)
                {
                    close(DialogResult::cancelled);
                }
            }
            return true;
        }
        if(mouse_event->pressed && row == 0 && column > 0 && from_right < 0)
        {
            m_dragging = true;
            m_drag_start_x = mouse_event->x;
            m_drag_start_y = mouse_event->y;
            m_drag_origin_x = m_drag_offset_x;
            m_drag_origin_y = m_drag_offset_y;
            return true;
        }
        if(mouse_event->dragging && m_dragging)
        {
            m_drag_offset_x = m_drag_origin_x + mouse_event->x - m_drag_start_x;
            m_drag_offset_y = m_drag_origin_y + mouse_event->y - m_drag_start_y;
            m_moved = true;
            return true;
        }
        if(mouse_event->released && m_dragging)
        {
            m_dragging = false;
            return true;
        }
        return false;
    }
    const auto* const key_event{std::get_if<KeyEvent>(&event)};
    if(key_event == nullptr)
    {
        return false;
    }
    if(key_event->key == Key::escape && m_close_on_escape)
    {
        close(DialogResult::cancelled);
        return true;
    }
    if(key_event->key == Key::enter && !key_event->control && !key_event->alt)
    {
        if(auto* const button{default_button(*this)})
        {
            button->click();
            return true;
        }
    }
    if(key_event->key == Key::enter && m_close_on_enter)
    {
        close(DialogResult::accepted);
        return true;
    }
    return false;
}

// Applies a size the user dragged to, clamped between the minimum and the room left below and to the right
// of the window, and moves the centring offset so the top-left corner stays where it is.
void Window::resize_to(Size requested)
{
    const auto left{bounds().x};
    const auto top{bounds().y};
    const auto room_width{m_area.x + m_area.width - left};
    const auto room_height{m_area.y + m_area.height - top};
    const Size size{std::clamp(requested.width, std::min(m_minimum_size.width, room_width), room_width),
                    std::clamp(requested.height, std::min(m_minimum_size.height, room_height), room_height)};
    m_user_size = size;
    m_drag_offset_x = left - (m_area.x + std::max(0.0, snap_x((m_area.width - size.width) / 2)));
    m_drag_offset_y = top - (m_area.y + std::max(0.0, snap_y((m_area.height - size.height) / 2)));
}

std::unique_ptr<Window> window(std::string_view title)
{
    return std::make_unique<Window>(title);
}

} // namespace olagarro::oui
