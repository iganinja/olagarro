#pragma once

#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <optional>
#include <string>
#include <vector>

namespace olagarro::oui
{

// How a graphical style looks, beyond the theme's colours: the shades of its raised and sunken shapes.
struct GraphicalStyle
{
    // What shows where nothing is drawn.
    Color desktop{44, 76, 64};
    // Raised surfaces: windows, buttons, bars. Light and shadow are their edges, light at the top and left.
    Color face{176, 168, 152};
    Color light{206, 199, 184};
    Color shadow{132, 124, 110};
    // A window's title bar, and its text.
    Color title{92, 124, 76};
    Color title_text{244, 244, 236};
    // Sunken surfaces, such as lists and text fields, and their text.
    Color well{34, 34, 32};
    Color well_text{208, 208, 200};
    // What has the keyboard focus, and a window placed from the keyboard.
    Color focus{228, 168, 44};
    // Text on raised surfaces, plain and quieter, and what is selected in a list.
    Color text{16, 16, 16};
    Color muted_text{88, 82, 72};
    Color selection{236, 236, 228};
    Color selection_text{16, 16, 16};
    // Icons of a file list: a folder and a page.
    Color folder{226, 178, 74};
    Color page{232, 228, 216};
    // How far interface text keeps from the edges of the cells it is set in.
    int text_margin{6};
    // The thickness of an edge, in pixels.
    int line{1};
};

// OUI's default graphical look, inspired by Transport Tycoon: soft bevelled grey surfaces, a green title, dark wells.
[[nodiscard]] GraphicalStyle classic_style();
// The theme that goes with a style: the colours widgets paint their content with.
[[nodiscard]] Theme theme_for(const GraphicalStyle& style);
[[nodiscard]] Theme classic_theme();

struct GraphicalTheme
{
    // Stays the same in every language, to keep in settings.
    std::string id;
    std::string name;
    GraphicalStyle style;
};

// The graphical looks built into OUI, the default first.
[[nodiscard]] std::vector<GraphicalTheme> graphical_themes();

// Paints the cells and elements of OUI on a canvas: the cells in a monospace font, interface text in a proportional
// one, and every element in a style of shapes rather than characters.
class GraphicalPainter final : public Painter
{
  public:
    // `symbols` draws what the other two lack, as symbol_font() does. The cells start at the top left of `area` and
    // nothing is drawn outside it; without one they take the whole canvas.
    GraphicalPainter(Canvas& canvas, const GraphicalStyle& style, Font& cell_font, Font& interface_font, Font& symbols,
                     std::optional<PixelRect> area = std::nullopt);

    // The size of a cell in pixels, from the cell font.
    [[nodiscard]] static Size cell_size(Font& cell_font);
    // How far from its left edge a text field's text starts.
    [[nodiscard]] static int field_padding(Size cell);

    void fill_rect(const Rect& rect, Color color) override;
    void draw_text(Point position, std::string_view text, const TextStyle& style) override;
    void push_clip(const Rect& rect) override;
    void pop_clip() override;

    void draw_label(Point position, std::string_view text, const TextStyle& style, Align align) override;
    void draw_label_runs(Point position, const std::vector<LabelRun>& runs, Color background) override;
    void draw_window(const Rect& bounds, const WindowLook& look, const Theme& theme) override;
    void draw_button(const Rect& bounds, const ButtonLook& look, const Theme& theme) override;
    void draw_check_box(const Rect& bounds, const CheckBoxLook& look, const Theme& theme) override;
    void draw_tab(const Rect& bounds, const TabLook& look, const Theme& theme) override;
    void draw_header(const Rect& cell, const HeaderLook& look, const Theme& theme) override;
    void draw_divider(const Rect& line, const Theme& theme) override;
    void draw_scrollbar(const Rect& track, const ScrollbarLook& look, const Theme& theme) override;
    void draw_bar(const Rect& bounds, BarKind kind, const Theme& theme) override;
    void draw_popup(const Rect& bounds, const Theme& theme) override;
    void draw_popup_edge(const Rect& bounds, const Theme& theme) override;
    void draw_popup_separator(const Rect& row, const Theme& theme) override;
    void draw_icon(Point position, Icon icon, const TextStyle& style) override;
    void draw_marker(Point position, const Marker& marker, const TextStyle& style) override;
    void draw_check_mark(Point position, bool checked, const TextStyle& style) override;
    void draw_tooltip(const Rect& bounds, const std::vector<std::string>& lines, const Theme& theme) override;
    void draw_drag_image(Point position, int cell_offset, std::string_view text, const Theme& theme) override;
    void draw_text_field(const Rect& bounds, const TextFieldLook& look, const Theme& theme) override;
    void draw_field_frame(const Rect& bounds, bool focused, const Theme& theme) override;
    void draw_image(const Rect& bounds, const PixelImage& image, const Theme& theme) override;
    void draw_mnemonic_label(Point position, std::string_view text, std::size_t mnemonic, const TextStyle& style,
                             Color mnemonic_color, Align align) override;

  private:
    [[nodiscard]] PixelRect pixels(const Rect& cells) const;
    // Light top-left and dark bottom-right edges, or the other way round when sunken, around `face`.
    void bevel(const PixelRect& rect, bool sunken, Color face);
    void edges(const PixelRect& rect, Color top_left, Color bottom_right);
    [[nodiscard]] const GlyphBitmap& cell_glyph(char32_t character);
    [[nodiscard]] const GlyphBitmap& interface_glyph(char32_t character);
    [[nodiscard]] int interface_width(std::string_view text);
    // Draws interface text in the pixels given, as `align` says; returns where it starts.
    int label(const PixelRect& area, std::string_view text, Color color, Align align, bool bold = false);
    void focus_ring(const PixelRect& rect);
    // A folder, a page or a folder to go up, in a square of `size` pixels at the top left of `at`.
    void draw_file_icon(Icon icon, PixelPoint at, float size);
    [[nodiscard]] PixelRect popup_area(const Rect& bounds) const;
    // A pixel of the shapes drawn for cells 24 pixels high, at the size of these cells.
    [[nodiscard]] float unit() const;
    // Moves interface text in from the edge it is aligned to.
    [[nodiscard]] PixelRect with_margin(const PixelRect& area, Align align) const;

    Canvas& m_canvas;
    const GraphicalStyle& m_style;
    Font& m_cell_font;
    Font& m_interface_font;
    FontSet m_fonts;
    Size m_cell;
    PixelRect m_area;
    std::vector<PixelRect> m_clips;
};

} // namespace olagarro::oui
