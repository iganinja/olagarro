#include <oui/number_field.h>

#include <oui/line_edit.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace olagarro::oui
{

namespace
{

// The arrows after the field: down, then up, a cell each.
constexpr int arrows_width{2};

} // namespace

NumberField::NumberField()
{
    m_field = &add(line_edit());
    // A number typed is the value at once, kept in range and on a step, without the text being rewritten under the
    // cursor; leaving the field shows it as it was kept.
    m_typed = m_field->on_changed(
        [this](std::string_view typed)
        {
            // The value shown is not taken back: shown with fewer decimals than it has, it would lose them.
            if(m_showing)
            {
                return;
            }
            if(const auto number{parse(typed)})
            {
                m_mixed = false;
                take(*number);
            }
        });
    m_field->set_key_handler(
        [this](const Event& event)
        {
            const auto* const key{std::get_if<KeyEvent>(&event)};
            if(key == nullptr)
            {
                return false;
            }
            switch(key->key)
            {
            case Key::up:
                step_by(1);
                return true;
            case Key::down:
                step_by(-1);
                return true;
            case Key::page_up:
                step_by(10);
                return true;
            case Key::page_down:
                step_by(-10);
                return true;
            default:
                return false;
            }
        });
    m_field->set_pointer_handler([this](const MouseEvent& mouse) { return drag_with(mouse); });
    show_value();
}

void NumberField::set_range(double minimum, double maximum)
{
    m_minimum = std::min(minimum, maximum);
    m_maximum = std::max(minimum, maximum);
    set_value(m_value);
}

void NumberField::set_step(double step)
{
    m_step = std::max(0.0, step);
    set_value(m_value);
}

void NumberField::set_draggable(bool draggable)
{
    m_draggable = draggable;
    m_drag.reset();
}

void NumberField::set_mixed(bool mixed)
{
    m_mixed = mixed;
    show_value();
}

bool NumberField::is_mixed() const
{
    return m_mixed;
}

bool NumberField::drag_with(const MouseEvent& mouse)
{
    if(!m_draggable || !is_enabled() || mouse.right || mouse.wheel_delta != 0)
    {
        return false;
    }
    if(mouse.pressed)
    {
        m_drag.reset();
        if(mouse.clicks == 2)
        {
            m_field->select_all();
            return true;
        }
        if(mouse.clicks != 1)
        {
            return false;
        }
        commit();
        m_drag = Drag{mouse, mouse.x, m_value};
        return true;
    }
    if(!m_drag)
    {
        return false;
    }
    if(mouse.released)
    {
        const auto drag{*m_drag};
        m_drag.reset();
        if(drag.moved)
        {
            m_drag_finished.fire();
        }
        else
        {
            // A click: the field puts its cursor where it fell.
            m_field->handle_event(drag.press);
            m_field->handle_event(mouse);
        }
        return true;
    }
    if(!mouse.dragging || mouse.x == m_drag->last_x)
    {
        return true;
    }
    if(!m_drag->moved)
    {
        m_drag->moved = true;
        m_mixed = false;
        m_drag_started.fire();
    }
    const auto step{m_step > 0.0 ? m_step : 1.0};
    const auto factor{mouse.control ? 10.0 : mouse.shift ? 0.1 : 1.0};
    m_drag->value = std::clamp(m_drag->value + (mouse.x - m_drag->last_x) * step * factor, m_minimum, m_maximum);
    m_drag->last_x = mouse.x;
    m_drag->fine = m_drag->fine || mouse.shift;
    keep(fitted(m_drag->value, m_drag->fine ? m_step / 10.0 : m_step));
    show_value();
    return true;
}

void NumberField::set_decimals(int decimals)
{
    m_decimals = std::clamp(decimals, 0, 9);
    show_value();
}

double NumberField::fitted(double value, double step) const
{
    if(std::isnan(value))
    {
        return m_minimum;
    }
    auto result{std::clamp(value, m_minimum, m_maximum)};
    if(step > 0.0)
    {
        result = std::min(m_maximum, m_minimum + std::round((result - m_minimum) / step) * step);
    }
    return result;
}

void NumberField::set_value(double value)
{
    m_mixed = false;
    take(value);
    show_value();
}

void NumberField::take(double value)
{
    keep(fitted(value, m_step));
}

void NumberField::keep(double value)
{
    if(value == m_value)
    {
        return;
    }
    m_value = value;
    m_changed.fire(m_value);
}

std::optional<double> NumberField::parse(std::string_view typed)
{
    const std::string text{typed};
    char* end{};
    const auto number{std::strtod(text.c_str(), &end)};
    if(text.empty() || end == text.c_str() || *end != '\0' || !std::isfinite(number))
    {
        return std::nullopt;
    }
    return number;
}

double NumberField::value() const
{
    return m_value;
}

LineEdit& NumberField::text_field()
{
    return *m_field;
}

void NumberField::step_by(double steps)
{
    commit();
    const auto step{m_step > 0.0 ? m_step : 1.0};
    // set_value takes it out of being mixed.
    set_value(m_value + step * steps);
}

void NumberField::commit()
{
    if(const auto number{parse(m_field->text())})
    {
        take(*number);
    }
    show_value();
}

void NumberField::show_value() const
{
    char text[64];
    std::snprintf(text, sizeof text, "%s", "—");
    if(!m_mixed)
    {
        std::snprintf(text, sizeof text, "%.*f", m_decimals, m_value);
    }
    if(m_field->text() != text)
    {
        m_showing = true;
        m_field->set_text(text);
        m_showing = false;
    }
}

Size NumberField::measure(const Constraints& constraints) const
{
    return constraints.constrain({8 + arrows_width, 1});
}

void NumberField::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    m_field->arrange({bounds.x, bounds.y, std::max(0, bounds.width - arrows_width), bounds.height});
}

void NumberField::paint(Painter& painter, const Theme& theme) const
{
    // Not being typed in, it shows its value, whatever was left typed.
    if(!m_field->is_focused())
    {
        show_value();
    }
    Container::paint(painter, theme);
    const Rect arrows{m_bounds.x + m_bounds.width - arrows_width, m_bounds.y, arrows_width, 1};
    painter.draw_spin_arrows(arrows, theme);
    // Where they are in pixels when one is over the other, for a click to tell which.
    m_stacked_arrows = painter.stacks_spin_arrows() ? painter.pixel_bounds(arrows) : std::nullopt;
}

bool NumberField::handle_event(const Event& event)
{
    // Enter, from the field, takes what was typed, and goes on, as to the window's default button.
    if(const auto* const key{std::get_if<KeyEvent>(&event)};
       key != nullptr && key->key == Key::enter && !key->control && !key->alt)
    {
        commit();
        return false;
    }
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    if(mouse == nullptr || !is_enabled())
    {
        return false;
    }
    if(mouse->wheel_delta != 0)
    {
        step_by(mouse->wheel_delta);
        return true;
    }
    if(!mouse->pressed || mouse->right)
    {
        return false;
    }
    const auto right{m_bounds.x + m_bounds.width};
    if(m_stacked_arrows && mouse->pixel && mouse->x >= right - arrows_width && mouse->x < right)
    {
        step_by(mouse->pixel->y < m_stacked_arrows->y + m_stacked_arrows->height / 2 ? 1 : -1);
        return true;
    }
    if(mouse->x == right - 2)
    {
        step_by(-1);
        return true;
    }
    if(mouse->x == right - 1)
    {
        step_by(1);
        return true;
    }
    return false;
}

std::unique_ptr<NumberField> number_field()
{
    return std::make_unique<NumberField>();
}

NumberField& number_field(Container& parent)
{
    return parent.add(number_field());
}

} // namespace olagarro::oui
