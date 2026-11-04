#include <paint/panels/color_panel.h>

#include <paint/panels/panel_color.h>

#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>

#include <cmath>
#include <string>
#include <utility>

namespace olagarro::paint
{

oui::Color on_screen(const Color& color)
{
    // Seen through, as over the panel's grey.
    const auto channel{[&color](double value) {
        return static_cast<std::uint8_t>(std::lround((value * color.alpha + 0.8 * (1.0 - color.alpha)) * 255.0));
    }};
    return {channel(color.red), channel(color.green), channel(color.blue)};
}

namespace
{

[[nodiscard]] Color gray(double value)
{
    return {value, value, value, 1.0};
}

} // namespace

ColorPanel::ColorPanel()
{
    // Greys from black to white first, as heightmaps need them, then the colours.
    for(int step{}; step <= 8; ++ step)
    {
        m_palette.push_back(gray(step / 8.0));
    }
    m_palette.push_back({0.0, 0.0, 0.0, 0.0});
    for(const auto& color : std::vector<Color>{{1, 0, 0, 1},
                                               {1, 0.5, 0, 1},
                                               {1, 1, 0, 1},
                                               {0.5, 1, 0, 1},
                                               {0, 1, 0, 1},
                                               {0, 1, 0.5, 1},
                                               {0, 1, 1, 1},
                                               {0, 0.5, 1, 1},
                                               {0, 0, 1, 1},
                                               {0.5, 0, 1, 1},
                                               {1, 0, 1, 1},
                                               {1, 0, 0.5, 1},
                                               {0.5, 0, 0, 1},
                                               {0.5, 0.25, 0, 1},
                                               {0, 0.5, 0, 1},
                                               {0, 0, 0.5, 1}})
    {
        m_palette.push_back(color);
    }
}

void ColorPanel::set_primary(const Color& color)
{
    m_primary = color;
    changed();
}

void ColorPanel::set_secondary(const Color& color)
{
    m_secondary = color;
    changed();
}

void ColorPanel::swap()
{
    std::swap(m_primary, m_secondary);
    changed();
}

void ColorPanel::on_changed(std::function<void()> changed)
{
    m_changed = std::move(changed);
}

void ColorPanel::changed() const
{
    if(m_changed)
    {
        m_changed();
    }
}

void ColorPanel::on_edit(std::function<void(bool primary)> edit)
{
    m_edit = std::move(edit);
}

void ColorPanel::on_picked(std::function<void(bool secondary)> picked)
{
    m_picked = std::move(picked);
}

std::optional<Color> ColorPanel::palette_at(oui::Point cell) const
{
    if(cell.y < squares_rows * m_scale || cell.x < 0 || cell.x >= width * m_scale)
    {
        return std::nullopt;
    }
    // Two cells to a colour, so each is square, two to a row; with more to a row, each as much smaller each way.
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto side{2.0 * m_scale * 2.0 / static_cast<double>(columns)};
    const auto row{static_cast<std::size_t>(std::floor((cell.y - squares_rows * m_scale) / (side / 2.0)))};
    const auto column{std::min(static_cast<std::size_t>(std::floor(cell.x / side)), columns - 1)};
    const auto index{row * columns + column};
    if(index >= m_palette.size())
    {
        return std::nullopt;
    }
    return m_palette[index];
}

oui::Size ColorPanel::measure(const oui::Constraints& constraints) const
{
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto rows{static_cast<double>((m_palette.size() + columns - 1) / columns)};
    return constraints.constrain({width * m_scale, (squares_rows + rows * 2.0 / static_cast<double>(columns)) * m_scale});
}

void ColorPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, side_panel_color(theme));
    // The primary square at the top left, the secondary down and to the right; the one being edited in front, framed.
    const auto s{m_scale};
    const oui::Rect primary{m_bounds.x, m_bounds.y, 3 * s, 2 * s};
    const oui::Rect secondary{m_bounds.x + s, m_bounds.y + s, 3 * s, 2 * s};
    const auto& front{m_editing_secondary ? secondary : primary};
    painter.fill_rect(m_editing_secondary ? primary : secondary,
                      on_screen(m_editing_secondary ? m_primary : m_secondary));
    painter.fill_rect(front, on_screen(m_editing_secondary ? m_secondary : m_primary));
    painter.draw_field_frame(front, true, theme);
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto side{2.0 * s * 2.0 / static_cast<double>(columns)};
    for(std::size_t index{}; index < m_palette.size(); ++ index)
    {
        const auto x{m_bounds.x + static_cast<double>(index % columns) * side};
        const auto y{m_bounds.y + squares_rows * s + static_cast<double>(index / columns) * side / 2.0};
        painter.fill_rect({x, y, side, side / 2.0}, on_screen(m_palette[index]));
    }
}

bool ColorPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed)
    {
        return false;
    }
    const oui::Point cell{mouse->x - m_bounds.x, mouse->y - m_bounds.y};
    if(cell.y < squares_rows * m_scale)
    {
        if(m_edit)
        {
            // The square in front is the one clicked where they overlap: the primary at the top left, the secondary
            // below and to its right.
            const auto on_primary{cell.y < 2 * m_scale && cell.x < 3 * m_scale};
            const auto on_secondary{cell.y >= m_scale && cell.x >= m_scale};
            m_edit(m_editing_secondary ? !on_secondary : on_primary);
        }
        return true;
    }
    if(const auto color{palette_at(cell)})
    {
        // The left button sets the colour being edited, the right one the other.
        const auto secondary{mouse->right != m_editing_secondary};
        (secondary ? m_secondary : m_primary) = *color;
        changed();
        if(m_picked)
        {
            m_picked(secondary);
        }
        return true;
    }
    return false;
}

void add_color_commands(oui::CommandRegistry& commands, ColorPanel& panel)
{
    commands.register_command("colors.swap", oui::translate("Commands", "Swap Colors"), [&panel] { panel.swap(); });
    commands.bind("colors.swap", oui::Shortcut{U'x'});
    commands.register_command("colors.toggle_editing", oui::translate("Commands", "Edit the Other Color"),
                              [&panel] { panel.toggle_editing(); });
    commands.bind("colors.toggle_editing", oui::Shortcut{U'c'});
}

} // namespace olagarro::paint
