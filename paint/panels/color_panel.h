#pragma once

#include <paint/color.h>

#include <oui/command.h>
#include <oui/widget.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace olagarro::paint
{

// A colour as a square of it shows on screen: seen through, as over a panel's grey.
[[nodiscard]] oui::Color on_screen(const Color& color);

// The primary and secondary colours, as two squares one over the other, and a palette under them: a click on a
// palette colour makes it the colour being edited, the primary unless C chose the secondary, a right click the other,
// and a click on a square asks for it. The colour being edited shows in front, framed.
class ColorPanel final : public oui::Widget
{
  public:
    static constexpr int width{4};

    ColorPanel();

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

  private:
    // Rows the squares take before the palette starts.
    static constexpr int squares_rows{3};

    Color m_primary{0.0, 0.0, 0.0, 1.0};
    Color m_secondary{1.0, 1.0, 1.0, 1.0};
    std::vector<Color> m_palette;
    bool m_editing_secondary{false};
    double m_scale{1.0};
    int m_palette_columns{2};
    std::function<void(bool)> m_edit;
    std::function<void(bool)> m_picked;
    std::function<void()> m_changed;

    void changed() const;
};

// Paint.NET's keys for the colours as commands bound in `commands`: X swaps the primary and the secondary
// ("colors.swap"), C chooses which of them the palette sets ("colors.toggle_editing"). The panel outlives the commands.
void add_color_commands(oui::CommandRegistry& commands, ColorPanel& panel);

} // namespace olagarro::paint
