#include <oui/menu.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>

#include <algorithm>
#include <string>

#include "markers.h"
namespace olagarro::oui
{
Menu::Menu(CommandRegistry& commands) : m_commands{commands}
{
}
void Menu::add_item(std::string_view command_id)
{
    if(const auto title{m_commands.title(command_id)})
    {
        m_items.push_back({std::string{command_id}, strip_mnemonic(*title)});
    }
}
bool Menu::is_focusable() const
{
    return true;
}
bool Menu::handle_event(const Event& event)
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || m_items.empty())
    {
        return false;
    }
    if(key->key == Key::down)
    {
        m_selected = m_items.empty() ? 0 : (m_selected + 1) % m_items.size();
        return true;
    }
    if(key->key == Key::up)
    {
        m_selected = m_items.empty() ? 0 : (m_selected + m_items.size() - 1) % m_items.size();
        return true;
    }
    if(key->key == Key::enter || key->key == Key::space)
    {
        return m_commands.execute(m_items.at(m_selected).command_id);
    }
    return false;
}
Size Menu::measure(const Constraints& constraints) const
{
    auto width{0.0};
    for(const auto& item : m_items)
    {
        const auto marker_width{m_commands.checked(item.command_id).has_value() ? 4 : 0};
        width = std::max(width, text_metrics().label_width(item.title) + marker_width);
    }
    return constraints.constrain({width, static_cast<double>(m_items.size())});
}
void Menu::paint(Painter& painter, const Theme& theme) const
{
    for(std::size_t index{}; index < m_items.size(); ++ index)
    {
        const auto selected{index == m_selected};
        const auto enabled{m_commands.enabled(m_items.at(index).command_id)};
        const TextStyle style{!enabled   ? theme.disabled_foreground
                              : selected ? theme.menu_selected_foreground
                                         : theme.menu_foreground,
                              selected ? theme.menu_selected_background : theme.menu_background};
        painter.fill_rect({bounds().x, bounds().y + static_cast<int>(index), bounds().width, 1}, style.background);
        const auto checked{m_commands.checked(m_items.at(index).command_id)};
        const auto y{bounds().y + static_cast<int>(index)};
        if(checked)
        {
            painter.draw_check_mark({bounds().x, y}, *checked, style);
        }
        const auto marker{checked ? text_width(check_marker(*checked)) : 0};
        painter.draw_label({bounds().x + marker, y, bounds().width - marker, 1}, m_items.at(index).title, style);
    }
}
std::unique_ptr<Menu> menu(CommandRegistry& commands)
{
    return std::make_unique<Menu>(commands);
}
Menu& menu(CommandRegistry& commands, Container& parent)
{
    return parent.add(menu(commands));
}
} // namespace olagarro::oui
