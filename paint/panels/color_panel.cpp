#include <paint/panels/color_panel.h>

#include <paint/panels/color_dialog.h>
#include <paint/panels/icons.h>
#include <paint/panels/panel_color.h>

#include <oui/dialogs.h>
#include <oui/file_dialog.h>
#include <oui/file_system.h>
#include <oui/painter.h>
#include <oui/theme.h>
#include <oui/translation.h>

#include <cmath>
#include <tuple>
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

// `text` with `{0}` in it replaced by `name`.
[[nodiscard]] std::string with_name(std::string text, const std::string& name)
{
    if(const auto place{text.find("{0}")}; place != std::string::npos)
    {
        text.replace(place, 3, name);
    }
    return text;
}

// Whether `point` is inside `rect`, its right and bottom edges left out.
[[nodiscard]] bool inside(const oui::Rect& rect, oui::Point point)
{
    return point.x >= rect.x && point.y >= rect.y && point.x < rect.x + rect.width && point.y < rect.y + rect.height;
}

} // namespace

PaletteAsks palette_asks(oui::Application& application,
                         std::function<void(std::vector<oui::MenuItem> items, oui::Point at)> open_menu)
{
    PaletteAsks asks;
    asks.color = [&application](const Color& start)
    { return ask_color(application, oui::translate("Palettes", "Palette Colour"), start); };
    asks.name = [&application](const std::string& title, const std::string& name)
    {
        oui::PromptOptions options;
        options.title = title;
        options.label = oui::translate("Palettes", "&Name:");
        options.text = name;
        return oui::prompt(application, options);
    };
    asks.confirm = [&application](const std::string& question)
    {
        return oui::message_box(application, oui::translate("Palettes", "Palettes"), question,
                                {oui::translate("Palettes", "Yes"), oui::translate("Palettes", "No")}) ==
               std::optional<std::size_t>{0};
    };
    asks.tell = [&application](const std::string& message)
    { static_cast<void>(oui::message_box(application, oui::translate("Palettes", "Palettes"), message)); };
    asks.gpl_file = [&application]() -> std::optional<std::string>
    {
        oui::FileDialogOptions options;
        options.title = oui::translate("Palettes", "Import a Palette");
        options.filters = {{oui::translate("Palettes", "GIMP palettes"), "*.gpl"}};
        const auto path{oui::file_dialog(application, options)};
        return path ? oui::read_file(*path) : std::nullopt;
    };
    asks.open_menu = std::move(open_menu);
    return asks;
}

void ColorPanel::set_palette(Palette palette)
{
    m_palette = std::move(palette);
    m_palette_modified = false;
    m_dragged.reset();
}

void ColorPanel::on_palette_changed(std::function<void()> changed)
{
    m_palette_changed = std::move(changed);
}

void ColorPanel::set_library(PaletteLibrary* library)
{
    m_library = library;
    m_dragged.reset();
}

void ColorPanel::set_asks(PaletteAsks asks)
{
    m_asks = std::move(asks);
}

void ColorPanel::palette_changed(bool modified)
{
    m_palette_modified = modified;
    if(m_palette_changed)
    {
        m_palette_changed();
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

double ColorPanel::palette_top() const
{
    return (squares_rows + (m_library != nullptr ? 1 : 0)) * m_scale;
}

oui::Rect ColorPanel::save_button() const
{
    return {m_bounds.x, m_bounds.y + squares_rows * m_scale, width / 2 * m_scale, m_scale};
}

oui::Rect ColorPanel::load_button() const
{
    return {m_bounds.x + width / 2 * m_scale, m_bounds.y + squares_rows * m_scale, width / 2 * m_scale, m_scale};
}

std::size_t ColorPanel::palette_cells() const
{
    return m_palette.colors.size() + (m_library != nullptr ? 1 : 0);
}

std::optional<std::size_t> ColorPanel::cell_index_at(oui::Point cell) const
{
    if(cell.y < palette_top() || cell.x < 0 || cell.x >= width * m_scale)
    {
        return std::nullopt;
    }
    // Two cells to a colour, so each is square, two to a row; with more to a row, each as much smaller each way.
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto side{2.0 * m_scale * 2.0 / static_cast<double>(columns)};
    const auto row{static_cast<std::size_t>(std::floor((cell.y - palette_top()) / (side / 2.0)))};
    const auto column{std::min(static_cast<std::size_t>(std::floor(cell.x / side)), columns - 1)};
    const auto index{row * columns + column};
    if(index >= palette_cells())
    {
        return std::nullopt;
    }
    return index;
}

std::optional<Color> ColorPanel::palette_at(oui::Point cell) const
{
    const auto index{cell_index_at(cell)};
    if(!index || *index >= m_palette.colors.size())
    {
        return std::nullopt;
    }
    return m_palette.colors[*index];
}

oui::Size ColorPanel::measure(const oui::Constraints& constraints) const
{
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto rows{static_cast<double>((palette_cells() + columns - 1) / columns)};
    return constraints.constrain(
        {width * m_scale, palette_top() + rows * 2.0 / static_cast<double>(columns) * m_scale});
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
    const oui::TextStyle text{theme.text, side_panel_color(theme)};
    if(m_library != nullptr)
    {
        // Save and Load over the colours, as the tools' buttons are drawn; letters where there are no pixels.
        for(const auto& [bounds, icon, letter] :
            {std::tuple{save_button(), "Save", "S"}, std::tuple{load_button(), "Load", "L"}})
        {
            const auto area{painter.pixel_bounds(bounds)};
            const auto* const picture{area && m_icons != nullptr
                                          ? m_icons->picture(icon, oui::whole(area->width), oui::whole(area->height))
                                          : nullptr};
            if(picture != nullptr)
            {
                oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
                image.alpha = true;
                painter.draw_image(bounds, image, theme);
            }
            else
            {
                painter.draw_text({bounds.x + std::floor((bounds.width - 1.0) / 2.0), bounds.y}, letter, text);
            }
        }
    }
    const auto columns{static_cast<std::size_t>(m_palette_columns)};
    const auto side{2.0 * s * 2.0 / static_cast<double>(columns)};
    for(std::size_t index{}; index < palette_cells(); ++ index)
    {
        const auto x{m_bounds.x + static_cast<double>(index % columns) * side};
        const auto y{m_bounds.y + palette_top() + static_cast<double>(index / columns) * side / 2.0};
        if(index < m_palette.colors.size())
        {
            painter.fill_rect({x, y, side, side / 2.0}, on_screen(m_palette.colors[index]));
        }
        else
        {
            painter.draw_text({x + std::floor((side - 1.0) / 2.0), y}, "+", text);
        }
    }
}

void ColorPanel::choose(const std::string& name)
{
    if(const auto palette{m_library->load(name)})
    {
        m_palette = *palette;
        palette_changed(false);
    }
    else if(m_asks.tell)
    {
        m_asks.tell(with_name(oui::translate("Palettes", "The palette {0} could not be read."), name));
    }
}

void ColorPanel::save_as(bool own_name_quietly)
{
    if(!m_asks.name)
    {
        return;
    }
    // Default cannot be saved over, so its name is not offered.
    const auto standard{m_palette.name == PaletteLibrary::default_name};
    const auto name{
        m_asks.name(oui::translate("Palettes", "Save the Palette As"), standard ? std::string{} : m_palette.name)};
    if(!name || name->empty())
    {
        return;
    }
    if(*name == PaletteLibrary::default_name)
    {
        if(m_asks.tell)
        {
            m_asks.tell(oui::translate("Palettes", "Default is Paint's own palette and cannot be replaced."));
        }
        return;
    }
    if(m_library->contains(*name) && !(own_name_quietly && *name == m_palette.name) &&
       !(m_asks.confirm && m_asks.confirm(with_name(oui::translate("Palettes", "Replace the palette {0}?"), *name))))
    {
        return;
    }
    auto saved{m_palette};
    saved.name = *name;
    if(!m_library->save(saved))
    {
        if(m_asks.tell)
        {
            m_asks.tell(with_name(oui::translate("Palettes", "The palette could not be saved in {0}."),
                                  oui::utf8_of(m_library->folder())));
        }
        return;
    }
    m_palette = std::move(saved);
    palette_changed(false);
}

void ColorPanel::rename()
{
    if(!m_asks.name)
    {
        return;
    }
    const auto name{m_asks.name(oui::translate("Palettes", "Rename the Palette"), m_palette.name)};
    if(!name || name->empty() || *name == m_palette.name)
    {
        return;
    }
    if(*name == PaletteLibrary::default_name)
    {
        if(m_asks.tell)
        {
            m_asks.tell(oui::translate("Palettes", "Default is Paint's own palette and cannot be replaced."));
        }
        return;
    }
    if(m_library->contains(*name))
    {
        if(!(m_asks.confirm &&
             m_asks.confirm(with_name(oui::translate("Palettes", "Replace the palette {0}?"), *name))))
        {
            return;
        }
        static_cast<void>(m_library->remove(*name));
    }
    if(m_library->contains(m_palette.name) && !m_library->rename(m_palette.name, *name))
    {
        if(m_asks.tell)
        {
            m_asks.tell(with_name(oui::translate("Palettes", "The palette could not be saved in {0}."),
                                  oui::utf8_of(m_library->folder())));
        }
        return;
    }
    m_palette.name = *name;
    palette_changed(m_palette_modified);
}

void ColorPanel::remove()
{
    if(!(m_asks.confirm &&
         m_asks.confirm(with_name(oui::translate("Palettes", "Remove the palette {0}?"), m_palette.name))))
    {
        return;
    }
    if(m_library->remove(m_palette.name))
    {
        // Its colours stay in the panel, saved nowhere now.
        palette_changed(true);
    }
}

void ColorPanel::import()
{
    if(!m_asks.gpl_file)
    {
        return;
    }
    const auto text{m_asks.gpl_file()};
    if(!text)
    {
        return;
    }
    auto palette{palette_from_gpl(*text)};
    if(!palette)
    {
        if(m_asks.tell)
        {
            m_asks.tell(oui::translate("Palettes", "That file is not a GIMP palette."));
        }
        return;
    }
    if(palette->name.empty() || palette->name == PaletteLibrary::default_name)
    {
        palette->name = oui::translate("Palettes", "Imported");
    }
    if(m_library->contains(palette->name) &&
       !(m_asks.confirm &&
         m_asks.confirm(with_name(oui::translate("Palettes", "Replace the palette {0}?"), palette->name))))
    {
        return;
    }
    if(!m_library->save(*palette) && m_asks.tell)
    {
        m_asks.tell(with_name(oui::translate("Palettes", "The palette could not be saved in {0}."),
                              oui::utf8_of(m_library->folder())));
    }
}

std::vector<oui::MenuItem> ColorPanel::palettes_items()
{
    std::vector<oui::MenuItem> items;
    for(const auto& name : m_library->names())
    {
        items.push_back(oui::menu_action(name, [this, name] { choose(name); }, true, name == m_palette.name));
    }
    items.push_back(oui::menu_separator());
    items.push_back(oui::menu_action(oui::translate("Palettes", "&Import a .gpl…"), [this] { import(); }));
    return items;
}

std::vector<oui::MenuItem> ColorPanel::menu_items()
{
    const auto standard{m_palette.name == PaletteLibrary::default_name};
    std::vector<oui::MenuItem> items{
        oui::menu_submenu(oui::translate("Palettes", "&Palettes"),
                          [this]
                          {
                              std::vector<oui::MenuItem> palettes;
                              for(const auto& name : m_library->names())
                              {
                                  palettes.push_back(oui::menu_action(
                                      name, [this, name] { choose(name); }, true, name == m_palette.name));
                              }
                              return palettes;
                          }),
        oui::menu_separator(),
        oui::menu_action(
            oui::translate("Palettes", "&Save"),
            [this]
            {
                if(m_library->save(m_palette))
                {
                    palette_changed(false);
                }
                else if(m_asks.tell)
                {
                    m_asks.tell(with_name(oui::translate("Palettes", "The palette could not be saved in {0}."),
                                          oui::utf8_of(m_library->folder())));
                }
            },
            !standard),
        oui::menu_action(oui::translate("Palettes", "Save &As…"), [this] { save_as(); }),
        oui::menu_action(
            oui::translate("Palettes", "&Rename…"), [this] { rename(); }, !standard),
        oui::menu_action(
            oui::translate("Palettes", "Re&move"), [this] { remove(); },
            !standard && m_library->contains(m_palette.name)),
        oui::menu_separator(),
        oui::menu_action(oui::translate("Palettes", "&Import a .gpl…"), [this] { import(); }),
        oui::menu_separator(),
        oui::menu_action(oui::translate("Palettes", "Add the &Primary Colour"),
                         [this]
                         {
                             m_palette.colors.push_back(m_primary);
                             palette_changed(true);
                         }),
    };
    return items;
}

bool ColorPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr)
    {
        return false;
    }
    const oui::Point cell{mouse->x - m_bounds.x, mouse->y - m_bounds.y};
    if(!mouse->pressed && (mouse->released || mouse->dragging))
    {
        if(!m_dragged)
        {
            return false;
        }
        if(mouse->released)
        {
            // Let go away from the panel, the colour goes; on another colour, it moves there.
            const auto from{*m_dragged};
            m_dragged.reset();
            const auto inside{cell.x >= 0 && cell.y >= 0 && cell.x < m_bounds.width && cell.y < m_bounds.height};
            const auto to{cell_index_at(cell)};
            if(!inside)
            {
                m_palette.colors.erase(m_palette.colors.begin() + static_cast<std::ptrdiff_t>(from));
                palette_changed(true);
            }
            else if(to && *to < m_palette.colors.size() && *to != from)
            {
                const auto color{m_palette.colors[from]};
                m_palette.colors.erase(m_palette.colors.begin() + static_cast<std::ptrdiff_t>(from));
                m_palette.colors.insert(m_palette.colors.begin() + static_cast<std::ptrdiff_t>(*to), color);
                palette_changed(true);
            }
        }
        return true;
    }
    if(!mouse->pressed)
    {
        return false;
    }
    m_dragged.reset();
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
    const auto index{cell_index_at(cell)};
    if(index && *index < m_palette.colors.size())
    {
        if(m_library != nullptr && !mouse->right && mouse->clicks >= 2)
        {
            // A double click asks for a colour in its place.
            if(const auto answer{m_asks.color ? m_asks.color(m_palette.colors[*index]) : std::nullopt})
            {
                m_palette.colors[*index] = *answer;
                palette_changed(true);
            }
            return true;
        }
        // The left button sets the colour being edited, the right one the other.
        const auto secondary{mouse->right != m_editing_secondary};
        (secondary ? m_secondary : m_primary) = m_palette.colors[*index];
        changed();
        if(m_picked)
        {
            m_picked(secondary);
        }
        if(m_library != nullptr && !mouse->right)
        {
            m_dragged = *index;
        }
        return true;
    }
    if(m_library == nullptr)
    {
        return false;
    }
    if(cell.y < palette_top() && !mouse->right)
    {
        // Save asks a name, Load opens the library's palettes under it.
        if(inside(save_button(), {mouse->x, mouse->y}))
        {
            save_as(true);
        }
        else if(inside(load_button(), {mouse->x, mouse->y}) && m_asks.open_menu)
        {
            const auto button{load_button()};
            m_asks.open_menu(palettes_items(), {button.x, button.y + button.height});
        }
        return true;
    }
    if(index && !mouse->right)
    {
        // The "+" after the colours.
        m_palette.colors.push_back(m_primary);
        palette_changed(true);
        return true;
    }
    if(mouse->right && m_asks.open_menu)
    {
        m_asks.open_menu(menu_items(), {mouse->x, mouse->y});
        return true;
    }
    return false;
}

std::optional<std::string> ColorPanel::tooltip_at(oui::Point point) const
{
    if(m_library == nullptr)
    {
        return std::nullopt;
    }
    // The palette's name, "*" after it while it changed since it was chosen or saved.
    const auto name{m_palette.name + (m_palette_modified ? "*" : "")};
    if(inside(save_button(), point))
    {
        return with_name(oui::translate("Palettes", "Save the palette {0}…"), name);
    }
    if(inside(load_button(), point))
    {
        return with_name(oui::translate("Palettes", "Choose a palette (now {0})"), name);
    }
    return std::nullopt;
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
