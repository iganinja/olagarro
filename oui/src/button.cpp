#include <oui/button.h>
#include <oui/command.h>
#include <oui/utf8.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>

namespace olagarro::oui
{

Button::Button(std::string_view text) : m_text{text}
{
}

void Button::set_text(std::string_view text)
{
    m_text = text;
}
void Button::set_shadow_visible(bool visible)
{
    m_shadow_visible = visible;
}
void Button::set_background(ColorRef background)
{
    m_background = background;
}
void Button::set_compact(bool compact)
{
    m_compact = compact;
}

void Button::set_default(bool is_default)
{
    m_default = is_default;
}

bool Button::is_default() const
{
    return m_default;
}

void Button::set_icon(std::optional<Icon> icon)
{
    m_icon = icon;
}

std::optional<Icon> Button::icon() const
{
    return m_icon;
}

std::string_view Button::text() const
{
    return m_text;
}

std::optional<char32_t> Button::mnemonic() const
{
    const auto offset{mnemonic_offset(m_text)};
    if(!offset)
    {
        return std::nullopt;
    }
    const auto shown{strip_mnemonic(m_text)};
    auto index{*offset};
    return decode_utf8(shown, index);
}

void Button::click()
{
    m_clicked.fire();
}

bool Button::is_focusable() const
{
    return true;
}

bool Button::handle_event(const Event& event)
{
    if(const auto* const mouse_event{std::get_if<MouseEvent>(&event)})
    {
        if(mouse_event->pressed)
        {
            m_pressed = true;
            return true;
        }
        if(mouse_event->released && m_pressed)
        {
            m_pressed = false;
            click();
            return true;
        }
        return false;
    }
    const auto* const key_event{std::get_if<KeyEvent>(&event)};
    if(key_event == nullptr || (key_event->key != Key::enter && key_event->key != Key::space))
    {
        return false;
    }

    click();
    return true;
}

Size Button::measure(const Constraints& constraints) const
{
    // Compact, the text and a cell either side on one row; otherwise room for the frame and the shadow below. Drawn in
    // pixels a button's text is raised round its margins, with no frame of characters beside it, but for an icon.
    const auto& metrics{text_metrics()};
    const auto width{metrics.label_width(strip_mnemonic(m_text))};
    const auto frame{metrics.cell_pixels() && !m_icon ? 0.0 : m_compact ? 2.0 : 5.0};
    return constraints.constrain({width + frame, m_compact ? 1.0 : 2.0});
}

void Button::paint(Painter& painter, const Theme& theme) const
{
    const auto shown{strip_mnemonic(m_text)};
    painter.draw_button(bounds(),
                        {shown, m_pressed, is_focused(), m_shadow_visible, m_compact, m_default,
                         mnemonic_offset(m_text), resolve(m_background, theme, theme.dialog_background), is_enabled(),
                         m_icon},
                        theme);
}

std::unique_ptr<Button> button(std::string_view text)
{
    return std::make_unique<Button>(text);
}

Button& button(std::string_view text, Container& parent)
{
    return parent.add(button(text));
}

} // namespace olagarro::oui
