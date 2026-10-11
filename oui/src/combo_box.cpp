#include <oui/combo_box.h>
#include <oui/utf8.h>
#include <oui/text_layout.h>

#include <algorithm>

namespace olagarro::oui
{

ComboBox::ComboBox(std::vector<std::string> options, std::size_t selected)
    : m_options{std::move(options)}, m_selected{m_options.empty() ? 0 : std::min(selected, m_options.size() - 1)}
{
}

void ComboBox::set_options(std::vector<std::string> options)
{
    m_options = std::move(options);
    m_selected = m_options.empty() ? 0 : std::min(m_selected, m_options.size() - 1);
    m_expanded = false;
    m_hovered.reset();
}

const std::vector<std::string>& ComboBox::options() const
{
    return m_options;
}

void ComboBox::set_selected(std::size_t index)
{
    if(index < m_options.size())
    {
        m_selected = index;
    }
}

std::size_t ComboBox::selected() const
{
    return m_selected;
}

bool ComboBox::is_focusable() const
{
    return true;
}

void ComboBox::choose(std::size_t index)
{
    if(index < m_options.size())
    {
        m_selected = index;
        m_selected_changed.fire(m_selected);
    }
}

bool ComboBox::handle_event(const Event& event)
{
    if(m_options.empty())
    {
        return false;
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(key->key == Key::escape && m_expanded)
        {
            m_expanded = false;
            return true;
        }
        if(key->key == Key::enter || key->key == Key::space)
        {
            // Closing on the option under the pointer picks it, as a click would.
            if(m_expanded && m_hovered.has_value())
            {
                choose(*m_hovered);
            }
            m_expanded = !m_expanded;
            m_hovered.reset();
            return true;
        }
        if(!m_expanded)
        {
            return false;
        }
        // The keyboard takes over from the pointer.
        m_hovered.reset();
        if(key->key == Key::up && m_selected > 0)
        {
            choose(m_selected - 1);
        }
        else if(key->key == Key::down && m_selected + 1 < m_options.size())
        {
            choose(m_selected + 1);
        }
        else if(key->key == Key::home)
        {
            choose(0);
        }
        else if(key->key == Key::end)
        {
            choose(m_options.size() - 1);
        }
        return true;
    }
    const auto* const mouse{std::get_if<MouseEvent>(&event)};
    if(mouse == nullptr)
    {
        return false;
    }
    const auto row{static_cast<std::size_t>(std::max(0, mouse->y - bounds().y))};
    if(mouse->hover && m_expanded)
    {
        m_hovered = std::min(row, m_options.size() - 1);
        return true;
    }
    if(!mouse->pressed)
    {
        return false;
    }
    if(!m_expanded)
    {
        m_expanded = true;
        m_hovered.reset();
        return true;
    }
    choose(row);
    m_expanded = false;
    return true;
}

Size ComboBox::measure(const Constraints& constraints) const
{
    auto width{0};
    for(const auto& option : m_options)
    {
        width = std::max(width, shown_width(option));
    }
    return constraints.constrain({width + 4, 1});
}

Rect ComboBox::paint_bounds() const
{
    return {bounds().x, bounds().y, bounds().width, m_expanded ? static_cast<int>(m_options.size()) : bounds().height};
}

Widget* ComboBox::hit_test(int x, int y)
{
    const auto area{paint_bounds()};
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height ? this : nullptr;
}

bool ComboBox::is_overlay() const
{
    // Only the open list spills over its neighbours; closed, it paints in its place, under a menu that opens over it.
    return m_expanded;
}

void ComboBox::dismiss_overlays_at(int x, int y)
{
    if(m_expanded && hit_test(x, y) == nullptr)
    {
        m_expanded = false;
        m_hovered.reset();
    }
}

void ComboBox::paint(Painter& painter, const Theme& theme) const
{
    // Focused like every other control; the open list marks the option under the pointer, or else its choice, with the
    // selection colours.
    const auto closed_background{is_focused() ? theme.focus_background : theme.table_background};
    const auto closed_foreground{!is_enabled()  ? theme.disabled_foreground
                                 : is_focused() ? theme.focus_text
                                                : theme.table_foreground};
    painter.fill_rect(paint_bounds(), closed_background);
    if(m_options.empty())
    {
        return;
    }
    const std::size_t first{m_expanded ? 0 : m_selected};
    const auto marked{m_hovered.value_or(m_selected)};
    const std::size_t count{m_expanded ? m_options.size() : 1};
    for(std::size_t row{}; row < count; ++ row)
    {
        const std::size_t option{first + row};
        const bool selected{option == marked};
        const auto background{m_expanded ? (selected ? theme.item_selected_background : theme.table_background)
                                         : closed_background};
        const auto foreground{m_expanded ? (selected ? theme.item_selected_foreground : theme.table_foreground)
                                         : closed_foreground};
        const int y{bounds().y + static_cast<int>(row)};
        painter.fill_rect({bounds().x, y, bounds().width, 1}, background);
        painter.draw_label({bounds().x + 1, y}, m_options.at(option), {foreground, background});
        if(!m_expanded)
        {
            // The arrow sits at the far right, so the field reads as a dropdown at a glance.
            painter.draw_icon({bounds().x + bounds().width - 2, y}, Icon::drop_down, {foreground, background});
        }
    }
    if(!m_expanded)
    {
        painter.draw_field_frame(bounds(), is_focused(), theme);
    }
}

std::unique_ptr<ComboBox> combo_box(std::vector<std::string> options, std::size_t selected)
{
    return std::make_unique<ComboBox>(std::move(options), selected);
}

ComboBox& combo_box(std::vector<std::string> options, std::size_t selected, Container& parent)
{
    return parent.add(combo_box(std::move(options), selected));
}

} // namespace olagarro::oui
