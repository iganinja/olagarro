#include <oui/check_button.h>
#include <oui/command.h>
#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/theme.h>
#include <oui/utf8.h>

namespace olagarro::oui
{

CheckButton::CheckButton(std::string_view text) : m_text{text}
{
}

void CheckButton::set_text(std::string_view text)
{
    m_text = text;
}
void CheckButton::set_background(ColorRef background)
{
    m_background = background;
}

std::string_view CheckButton::text() const
{
    return m_text;
}

void CheckButton::set_checked(bool checked)
{
    m_checked = checked;
}

bool CheckButton::is_checked() const
{
    return m_checked;
}

bool CheckButton::is_focusable() const
{
    return true;
}

void CheckButton::toggle()
{
    m_checked = !m_checked;
    m_toggled.fire(m_checked);
}

std::optional<char32_t> CheckButton::mnemonic() const
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

bool CheckButton::handle_event(const Event& event)
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(!key->control && !key->alt)
        {
            // Space ticks it; Enter is the dialog's, for its default button, as in Turbo Vision.
            if(key->key == Key::space)
            {
                toggle();
                return true;
            }
        }
        return false;
    }

    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(mouse->y != bounds().y || mouse->x < bounds().x || mouse->x >= bounds().x + bounds().width)
        {
            return false;
        }
        if(mouse->pressed)
        {
            m_pressed = true;
            return true;
        }
        if(m_pressed)
        {
            toggle();
            m_pressed = false;
            return true;
        }
        return m_pressed;
    }

    return false;
}

Size CheckButton::measure(const Constraints& constraints) const
{
    const double width{static_cast<double>(shown_width(strip_mnemonic(m_text)) + 5)};
    return constraints.constrain({width, 1.0});
}

void CheckButton::paint(Painter& painter, const Theme& theme) const
{
    painter.draw_check_box(bounds(),
                           {strip_mnemonic(m_text), m_checked, is_focused(),
                            resolve(m_background, theme, theme.dialog_background), mnemonic_offset(m_text),
                            is_enabled()},
                           theme);
}

std::unique_ptr<CheckButton> check_button(std::string_view text)
{
    return std::make_unique<CheckButton>(text);
}

CheckButton& check_button(std::string_view text, Container& parent)
{
    return parent.add(std::make_unique<CheckButton>(text));
}

} // namespace olagarro::oui
