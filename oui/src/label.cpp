#include <oui/label.h>
#include <oui/command.h>
#include <oui/text_layout.h>
#include <oui/utf8.h>

#include <algorithm>

namespace olagarro::oui
{

Label::Label(std::string_view text) : m_text{text}
{
}
void Label::set_text(std::string_view text)
{
    m_text = text;
}
void Label::set_wrap(bool enabled)
{
    m_wrap = enabled;
}
void Label::set_centered(bool enabled)
{
    m_centered = enabled;
}
void Label::set_right_aligned(bool enabled)
{
    m_right_aligned = enabled;
}
void Label::set_color(ColorRef color)
{
    m_color = color;
}
void Label::set_background(ColorRef background)
{
    m_background = background;
}
void Label::set_bold(bool enabled)
{
    m_bold = enabled;
}
bool Label::wraps() const
{
    return m_wrap;
}
bool Label::is_centered() const
{
    return m_centered;
}
bool Label::is_right_aligned() const
{
    return m_right_aligned;
}
std::optional<ColorRef> Label::color() const
{
    return m_color;
}
bool Label::is_bold() const
{
    return m_bold;
}
std::string_view Label::text() const
{
    return m_text;
}

void Label::set_marker(std::string text, int columns, std::optional<Icon> icon)
{
    m_marker_text = std::move(text);
    m_marker_columns = columns;
    m_marker_icon = icon;
    m_has_marker = true;
}

std::optional<Marker> Label::marker() const
{
    if(!m_has_marker)
    {
        return std::nullopt;
    }
    return Marker{m_marker_text, m_marker_columns, m_marker_icon};
}

Size Label::measure(const Constraints& constraints) const
{
    if(!m_wrap)
    {
        return constraints.constrain(
            {(m_has_marker ? m_marker_columns : 0) + shown_width(buddy() != nullptr ? strip_mnemonic(m_text) : m_text),
             1});
    }
    const auto width{std::max(1, std::min(shown_width(m_text), constraints.max_width))};
    return constraints.constrain({width, static_cast<int>(wrap_text(m_text, width).size())});
}

void Label::paint(Painter& painter, const Theme& theme) const
{
    if(!is_visible())
    {
        return;
    }
    const auto selected{parent() != nullptr && parent()->is_selected_child(*this)};
    const auto foreground{m_color    ? m_color->resolve(theme)
                          : selected ? theme.selection_foreground
                                     : theme.dialog_foreground};
    TextStyle style{foreground,
                    selected ? theme.selection_background : resolve(m_background, theme, theme.dialog_background)};
    if(m_bold)
    {
        style.attributes |= text_bold;
    }
    if(!m_wrap)
    {
        const auto offset{m_centered        ? std::max(0, (bounds().width - shown_width(m_text)) / 2)
                          : m_right_aligned ? std::max(0, bounds().width - shown_width(m_text))
                                            : 0};
        if(const auto mark{marker()})
        {
            painter.draw_marker({bounds().x, bounds().y}, *mark, style);
            painter.draw_label({bounds().x + mark->columns, bounds().y}, m_text, style);
            return;
        }
        if(buddy() != nullptr)
        {
            const auto shown{strip_mnemonic(m_text)};
            const auto room{bounds().width - shown_width(shown)};
            const Point at{bounds().x + (m_centered        ? std::max(0, room / 2)
                                         : m_right_aligned ? std::max(0, room)
                                                           : 0),
                           bounds().y};
            if(const auto letter{mnemonic_offset(m_text)})
            {
                painter.draw_mnemonic_label(at, shown, *letter, style, theme.label_shortcut_foreground);
                return;
            }
            painter.draw_label(at, shown, style);
            return;
        }
        painter.draw_label({bounds().x + offset, bounds().y}, m_text, style,
                           m_centered        ? Align::center
                           : m_right_aligned ? Align::end
                                             : Align::start);
        return;
    }
    const auto lines{wrap_text(m_text, bounds().width)};
    const auto total_height{static_cast<int>(lines.size())};
    const auto y_offset{m_centered ? std::max(0, (bounds().height - total_height) / 2) : 0};
    for(std::size_t index{}; index < lines.size(); ++ index)
    {
        const auto line_offset{m_centered ? std::max(0, (bounds().width - shown_width(lines.at(index))) / 2) : 0};
        painter.draw_label({bounds().x + line_offset, bounds().y + y_offset + static_cast<int>(index)}, lines.at(index),
                           style, m_centered ? Align::center : Align::start);
    }
}

void Label::set_buddy(Widget& buddy)
{
    m_buddy = &buddy;
}

Widget* Label::buddy() const
{
    return m_buddy.get();
}

std::optional<char32_t> Label::mnemonic() const
{
    const auto offset{buddy() != nullptr ? mnemonic_offset(m_text) : std::nullopt};
    if(!offset)
    {
        return std::nullopt;
    }
    const auto shown{strip_mnemonic(m_text)};
    auto index{*offset};
    return decode_utf8(shown, index);
}

std::unique_ptr<Label> label(std::string_view text)
{
    return std::make_unique<Label>(text);
}
Label& label(std::string_view text, Container& parent)
{
    return parent.add(label(text));
}

} // namespace olagarro::oui
