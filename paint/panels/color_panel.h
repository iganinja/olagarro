#pragma once

#include <paint/color.h>
#include <paint/palette.h>
#include <paint/panels/palette_library.h>

#include <oui/command.h>
#include <oui/menu_bar.h>
#include <oui/widget.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui
{
class Application;
}

namespace olagarro::paint
{

class Icons;

// A colour as a square of it shows on screen: seen through, as over a panel's grey.
[[nodiscard]] oui::Color on_screen(const Color& color);

// What a colour panel asks a person while it edits palettes. Each may be left empty: what needs it then does nothing.
struct PaletteAsks
{
    // A colour, from `start`; none when cancelled.
    std::function<std::optional<Color>(const Color& start)> color;
    // A palette's name, asked under `title`, from `name`; none when cancelled.
    std::function<std::optional<std::string>(const std::string& title, const std::string& name)> name;
    // Whether to go on, asked as a yes or no.
    std::function<bool(const std::string& question)> confirm;
    // Says what went wrong.
    std::function<void(const std::string& message)> tell;
    // The text of a .gpl file the person chose; none when cancelled.
    std::function<std::optional<std::string>()> gpl_file;
    // Opens a menu with its top left corner at `at`, as `oui::MenuBar::open_context_menu` does.
    std::function<void(std::vector<oui::MenuItem> items, oui::Point at)> open_menu;
};

// The asks with Paint's colour dialog (`ask_color`), OUI's prompt, message boxes and file dialog, the menus opened by
// `open_menu`. The application outlives them.
[[nodiscard]] PaletteAsks palette_asks(oui::Application& application,
                                       std::function<void(std::vector<oui::MenuItem> items, oui::Point at)> open_menu);

// The primary and secondary colours, as two squares one over the other, and a palette under them: a click on a
// palette colour makes it the colour being edited, the primary unless C chose the secondary, a right click the other,
// and a click on a square asks for it. The colour being edited shows in front, framed.
//
// Given a library of palettes, the palette is one's own to change and keep by name: two buttons over its colours save
// it as a name and choose one of the library's (their tooltips say its name, with "*" while it changed since it was
// chosen or saved); a double click on a colour asks for another in its place, the cell after the last adds the primary
// colour, a colour dragged away from the panel goes and dragged onto another moves there, and a right click away from
// the colours opens a menu of the library's palettes, to choose, save, rename, remove and import them.
class ColorPanel final : public oui::Widget
{
  public:
    static constexpr int width{4};

    // The palette under the squares, `Palette::standard()` at first.
    [[nodiscard]] const Palette& palette() const { return m_palette; }
    // Shows another palette as it is, unchanged since chosen, without telling.
    void set_palette(Palette palette);
    // Whether the palette changed since it was set, chosen or saved.
    [[nodiscard]] bool palette_modified() const { return m_palette_modified; }
    // Called after the panel changed the palette, its colours or its name, as a person edited or chose it.
    void on_palette_changed(std::function<void()> changed);
    // The library the panel edits, which outlives it; none, as at first, leaves the palette only to pick from.
    void set_library(PaletteLibrary* library);
    void set_asks(PaletteAsks asks);
    // The icons its buttons are drawn with ("Save" and "Load"), which outlive it; without them, or in a terminal,
    // they are letters.
    void set_icons(Icons* icons) { m_icons = icons; }

    [[nodiscard]] const Color& primary() const { return m_primary; }
    [[nodiscard]] const Color& secondary() const { return m_secondary; }
    void set_primary(const Color& color);
    void set_secondary(const Color& color);
    void swap();
    // Whether a click on the palette sets the secondary colour, as C toggles it.
    [[nodiscard]] bool editing_secondary() const { return m_editing_secondary; }
    void toggle_editing() { m_editing_secondary = !m_editing_secondary; }
    // Called with true for the primary square, false for the secondary, when it is clicked.
    void on_edit(std::function<void(bool primary)> edit);
    // Called after a palette colour was clicked: true for the secondary.
    void on_picked(std::function<void(bool secondary)> picked);
    // Called after the primary or the secondary colour changed, however it changed.
    void on_changed(std::function<void()> changed);

    // How many times bigger than its cells it is drawn, squares and palette; 1 at first, and in a terminal.
    void set_scale(double scale) { m_scale = std::max(1.0, scale); }
    [[nodiscard]] double scale() const { return m_scale; }

    // How many palette colours to a row, 2 at first: more makes each smaller, as square as two cells to a row are, so a
    // long palette takes fewer rows. A terminal keeps 2, its cells being whole.
    void set_palette_columns(int columns) { m_palette_columns = std::max(1, columns); }

    // The palette colour at a cell inside the panel, counted from its top left corner.
    [[nodiscard]] std::optional<Color> palette_at(oui::Point cell) const;

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override;
    void paint(oui::Painter& painter, const oui::Theme& theme) const override;
    bool handle_event(const oui::Event& event) override;
    [[nodiscard]] std::optional<std::string> tooltip_at(oui::Point point) const override;

  private:
    // Rows the squares take before the palette starts.
    static constexpr int squares_rows{3};

    Color m_primary{0.0, 0.0, 0.0, 1.0};
    Color m_secondary{1.0, 1.0, 1.0, 1.0};
    Palette m_palette{Palette::standard()};
    bool m_palette_modified{false};
    bool m_editing_secondary{false};
    double m_scale{1.0};
    int m_palette_columns{2};
    PaletteLibrary* m_library{};
    PaletteAsks m_asks;
    Icons* m_icons{};
    // The colour pressed on with the left button, while it may be dragged.
    std::optional<std::size_t> m_dragged;
    std::function<void(bool)> m_edit;
    std::function<void(bool)> m_picked;
    std::function<void()> m_changed;
    std::function<void()> m_palette_changed;

    void changed() const;
    // The palette's colours start under the squares and, with a library, the buttons.
    [[nodiscard]] double palette_top() const;
    // With a library, the row of buttons under the squares: Save at the left, Load at the right, each as wide as a
    // tool's button.
    [[nodiscard]] oui::Rect save_button() const;
    [[nodiscard]] oui::Rect load_button() const;
    // The menu of the library's palettes, to choose one, and Import.
    [[nodiscard]] std::vector<oui::MenuItem> palettes_items();
    // Cells of the palette's grid: its colours and, with a library, the "+" after them.
    [[nodiscard]] std::size_t palette_cells() const;
    [[nodiscard]] std::optional<std::size_t> cell_index_at(oui::Point cell) const;
    // The palette changed by the panel, `modified` as it is now.
    void palette_changed(bool modified);
    [[nodiscard]] std::vector<oui::MenuItem> menu_items();
    void choose(const std::string& name);
    // Asks a name and saves the palette as it, asking before replacing another; from the Save button, saved over its
    // own name without asking.
    void save_as(bool own_name_quietly = false);
    void rename();
    void remove();
    void import();
};

// Paint.NET's keys for the colours as commands bound in `commands`: X swaps the primary and the secondary
// ("colors.swap"), C chooses which of them the palette sets ("colors.toggle_editing"). The panel outlives the commands.
void add_color_commands(oui::CommandRegistry& commands, ColorPanel& panel);

} // namespace olagarro::paint
