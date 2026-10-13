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
    m_submitted = m_field->on_submitted([this](std::string_view) { commit(); });
    // A number typed is the value at once, kept in range and on a step, without the text being rewritten under the
    // cursor; leaving the field shows it as it was kept.
    m_typed = m_field->on_changed(
        [this](std::string_view typed)
        {
            if(const auto number{parse(typed)})
            {
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

void NumberField::set_decimals(int decimals)
{
    m_decimals = std::clamp(decimals, 0, 9);
    show_value();
}

double NumberField::fitted(double value) const
{
    if(std::isnan(value))
    {
        return m_minimum;
    }
    auto result{std::clamp(value, m_minimum, m_maximum)};
    if(m_step > 0.0)
    {
        result = std::min(m_maximum, m_minimum + std::round((result - m_minimum) / m_step) * m_step);
    }
    return result;
}

void NumberField::set_value(double value)
{
    take(value);
    show_value();
}

void NumberField::take(double value)
{
    const auto fitted_value{fitted(value)};
    if(fitted_value == m_value)
    {
        return;
    }
    m_value = fitted_value;
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
    std::snprintf(text, sizeof text, "%.*f", m_decimals, m_value);
    if(m_field->text() != text)
    {
        m_field->set_text(text);
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
    const auto right{m_bounds.x + m_bounds.width};
    // As icons, drawn at any size, not as characters a font may lack.
    const TextStyle arrows{theme.button_foreground, theme.button_background};
    painter.draw_icon({right - 2, m_bounds.y}, Icon::sorted_down, arrows);
    painter.draw_icon({right - 1, m_bounds.y}, Icon::sorted_up, arrows);
}

bool NumberField::handle_event(const Event& event)
{
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
