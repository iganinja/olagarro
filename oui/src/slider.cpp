#include <oui/slider.h>

#include <oui/container.h>
#include <oui/painter.h>

#include <algorithm>
#include <cmath>

namespace olagarro::oui
{

void Slider::set_range(double minimum, double maximum)
{
    m_minimum = std::min(minimum, maximum);
    m_maximum = std::max(minimum, maximum);
    set_value(m_value);
}

double Slider::minimum() const
{
    return m_minimum;
}

double Slider::maximum() const
{
    return m_maximum;
}

void Slider::set_step(double step)
{
    m_step = std::max(0.0, step);
    set_value(m_value);
}

double Slider::step() const
{
    return m_step;
}

double Slider::fitted(double value) const
{
    if(std::isnan(value))
    {
        return m_minimum;
    }
    auto result{std::clamp(value, m_minimum, m_maximum)};
    if(m_step > 0.0)
    {
        result = m_minimum + std::round((result - m_minimum) / m_step) * m_step;
        // A last step shorter than the others still reaches the maximum.
        result = std::min(result, m_maximum);
    }
    return result;
}

void Slider::set_value(double value)
{
    const auto fitted_value{fitted(value)};
    if(fitted_value == m_value)
    {
        return;
    }
    m_value = fitted_value;
    m_changed.fire(m_value);
}

double Slider::value() const
{
    return m_value;
}

Size Slider::measure(const Constraints& constraints) const
{
    return constraints.constrain({16, 1});
}

void Slider::paint(Painter& painter, const Theme& theme) const
{
    m_pixels = painter.pixel_bounds(m_bounds);
    const auto span{m_maximum - m_minimum};
    painter.draw_slider(m_bounds, {span > 0.0 ? (m_value - m_minimum) / span : 0.0, is_focused(), is_enabled()}, theme);
}

bool Slider::is_focusable() const
{
    return is_enabled();
}

bool Slider::takes_raw_pointer() const
{
    return true;
}

double Slider::value_at(const MouseEvent& mouse) const
{
    auto fraction{0.0};
    if(mouse.pixel && m_pixels && m_pixels->width > 1)
    {
        // The thumb's middle under the pointer, and the thumb as wide as the track is tall.
        const auto thumb{std::min(m_pixels->height, std::floor(m_pixels->width / 2))};
        const auto travel{std::max(1.0, m_pixels->width - thumb)};
        fraction = (mouse.pixel->x - m_pixels->x - std::floor(thumb / 2)) / travel;
    }
    else if(m_bounds.width > 1)
    {
        fraction = static_cast<double>(mouse.x - m_bounds.x) / (m_bounds.width - 1);
    }
    return m_minimum + std::clamp(fraction, 0.0, 1.0) * (m_maximum - m_minimum);
}

bool Slider::claims_key(const Event& event) const
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || key->control || key->alt || !is_enabled())
    {
        return false;
    }
    switch(key->key)
    {
    case Key::left:
    case Key::right:
    case Key::up:
    case Key::down:
    case Key::page_up:
    case Key::page_down:
    case Key::home:
    case Key::end:
        return true;
    default:
        return false;
    }
}

bool Slider::handle_event(const Event& event)
{
    if(!is_enabled())
    {
        return false;
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        const auto step{m_step > 0.0 ? m_step : (m_maximum - m_minimum) / 100.0};
        const auto page{std::max(step, (m_maximum - m_minimum) / 10.0)};
        switch(key->key)
        {
        case Key::left:
        case Key::down:
            set_value(m_value - step);
            return true;
        case Key::right:
        case Key::up:
            set_value(m_value + step);
            return true;
        case Key::page_down:
            set_value(m_value - page);
            return true;
        case Key::page_up:
            set_value(m_value + page);
            return true;
        case Key::home:
            set_value(m_minimum);
            return true;
        case Key::end:
            set_value(m_maximum);
            return true;
        default:
            return false;
        }
    }
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    if(mouse == nullptr || mouse->right || mouse->middle || mouse->right_dragging || mouse->middle_dragging)
    {
        return false;
    }
    if(mouse->wheel_delta != 0)
    {
        const auto step{m_step > 0.0 ? m_step : (m_maximum - m_minimum) / 100.0};
        set_value(m_value + step * mouse->wheel_delta);
        return true;
    }
    if(mouse->pressed || mouse->dragging)
    {
        set_value(value_at(*mouse));
        return true;
    }
    return mouse->released;
}

std::unique_ptr<Slider> slider()
{
    return std::make_unique<Slider>();
}

Slider& slider(Container& parent)
{
    return parent.add(slider());
}

} // namespace olagarro::oui
