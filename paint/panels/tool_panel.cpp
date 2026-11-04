#include <paint/panels/tool_panel.h>

#include <paint/panels/panel_color.h>

#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>

#include <cmath>
#include <utility>

namespace olagarro::paint
{

std::vector<ToolButton> paint_tool_buttons()
{
    return {
        {"rectangle_select", "RegionEdit", oui::translate("Tools", "Rectangle Select"), U's'},
        {"move_pixels", "ToolMove", oui::translate("Tools", "Move Selected Pixels"), U'm'},
        {"lasso_select", "Lasso", oui::translate("Tools", "Lasso Select"), U's'},
        {"move_selection", "ToolSelect", oui::translate("Tools", "Move Selection"), U'm'},
        {"ellipse_select", "EllipseSelect", oui::translate("Tools", "Ellipse Select"), U's'},
        {"magic_wand", "MagicWand", oui::translate("Tools", "Magic Wand"), U's'},
        {"bucket", "Bucket", oui::translate("Tools", "Paint Bucket"), U'f'},
        {"gradient", "Gradient", oui::translate("Tools", "Gradient"), U'g'},
        {"brush", "Paintbrush", oui::translate("Tools", "Paintbrush"), U'b'},
        {"eraser", "Eraser", oui::translate("Tools", "Eraser"), U'e'},
        {"pencil", "Edit", oui::translate("Tools", "Pencil"), U'p'},
        {"color_picker", "ColorPick", oui::translate("Tools", "Color Picker"), U'k'},
        {"line", "Line", oui::translate("Tools", "Line / Curve"), U'o'},
        {"shapes", "Shapes", oui::translate("Tools", "Shapes"), U'o'},
        {"arc", "Arc", oui::translate("Tools", "Arc"), U'o'},
    };
}

std::optional<std::string> tool_for_key(const std::vector<ToolButton>& buttons, char32_t key, std::string_view current)
{
    // The order the key goes through them is Paint.NET's: rectangle, lasso, ellipse, magic wand.
    std::vector<const ToolButton*> sharing;
    for(const auto& button : buttons)
    {
        if(button.key == key)
        {
            sharing.push_back(&button);
        }
    }
    if(sharing.empty())
    {
        return std::nullopt;
    }
    for(std::size_t index{}; index < sharing.size(); ++ index)
    {
        if(sharing[index]->id == current)
        {
            return sharing[(index + 1) % sharing.size()]->id;
        }
    }
    return sharing.front()->id;
}

ToolPanel::ToolPanel(Icons& icons) : ToolPanel{paint_tool_buttons(), icons}
{
}

ToolPanel::ToolPanel(std::vector<ToolButton> buttons, Icons& icons) : m_buttons{std::move(buttons)}, m_icons{icons}
{
    if(!m_buttons.empty())
    {
        m_selected = m_buttons.front().id;
    }
}

void ToolPanel::set_selected(std::string_view id)
{
    m_selected = std::string{id};
}

void ToolPanel::set_selected(Tool tool)
{
    set_selected(tool_id(tool));
}

const std::string& ToolPanel::selected() const
{
    return m_selected;
}

std::optional<Tool> ToolPanel::selected_tool() const
{
    return tool_from_id(m_selected);
}

bool ToolPanel::press_key(char32_t key)
{
    auto next{tool_for_key(m_buttons, key, m_selected)};
    if(!next)
    {
        return false;
    }
    pick(std::move(*next));
    return true;
}

void ToolPanel::on_picked(std::function<void(const std::string&)> picked)
{
    m_picked = std::move(picked);
}

void ToolPanel::pick(std::string id)
{
    m_selected = std::move(id);
    if(m_picked)
    {
        m_picked(m_selected);
    }
}

std::optional<std::size_t> ToolPanel::button_at(oui::Point cell) const
{
    const auto width{button_width * m_scale};
    if(cell.x < 0 || cell.y < 0 || cell.x >= columns * width)
    {
        return std::nullopt;
    }
    const auto row{static_cast<std::size_t>(std::floor(cell.y / m_scale))};
    const auto column{static_cast<std::size_t>(std::floor(cell.x / width))};
    const auto index{row * columns + column};
    if(index >= m_buttons.size())
    {
        return std::nullopt;
    }
    return index;
}

oui::Rect ToolPanel::button_bounds(std::size_t index) const
{
    const auto column{static_cast<int>(index) % columns};
    const auto row{static_cast<int>(index) / columns};
    const auto width{button_width * m_scale};
    return {m_bounds.x + column * width, m_bounds.y + row * m_scale, width, m_scale};
}

oui::Size ToolPanel::measure(const oui::Constraints& constraints) const
{
    const auto rows{(static_cast<int>(m_buttons.size()) + columns - 1) / columns};
    return constraints.constrain({columns * button_width * m_scale, rows * m_scale});
}

void ToolPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, side_panel_color(theme));
    for(std::size_t index{}; index < m_buttons.size(); ++ index)
    {
        const auto bounds{button_bounds(index)};
        if(m_buttons[index].id == m_selected)
        {
            painter.fill_rect(bounds, theme.menu_selected_background);
        }
        const auto area{painter.pixel_bounds(bounds)};
        if(!area)
        {
            continue;
        }
        const auto selected{m_buttons[index].id == m_selected};
        if(const auto* const picture{
               m_icons.picture(m_buttons[index].icon, oui::whole(area->width), oui::whole(area->height),
                               selected ? std::optional<oui::Color>{theme.menu_selected_background} : std::nullopt)})
        {
            oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
            image.alpha = true;
            painter.draw_image(bounds, image, theme);
        }
    }
}

bool ToolPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed || mouse->right)
    {
        return false;
    }
    const auto index{button_at({mouse->x - m_bounds.x, mouse->y - m_bounds.y})};
    if(!index)
    {
        return false;
    }
    pick(m_buttons[*index].id);
    return true;
}

std::optional<std::string> ToolPanel::tooltip_at(oui::Point point) const
{
    if(const auto index{button_at({point.x - m_bounds.x, point.y - m_bounds.y})})
    {
        const auto& button{m_buttons[*index]};
        const auto key{static_cast<char>(button.key - U'a' + U'A')};
        return button.title + " (" + std::string(1, key) + ")";
    }
    return std::nullopt;
}

void add_tool_commands(oui::CommandRegistry& commands, ToolPanel& panel)
{
    // Each tool's letter, and again for the next tool with the same letter.
    for(const auto& button : panel.buttons())
    {
        const auto id{std::string{"tool.key_"} + static_cast<char>(button.key)};
        if(commands.shortcut(id))
        {
            continue;
        }
        commands.register_command(id, button.title, [&panel, key{button.key}] { panel.press_key(key); });
        commands.bind(id, oui::Shortcut{button.key});
    }
}

} // namespace olagarro::paint
