#include <oui/graphics/graphical_painter.h>
#include <oui/text_layout.h>
#include <oui/translation.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "../unicode_composition.h"

namespace olagarro::oui
{

namespace
{

[[nodiscard]] Color mix(Color from, Color to, int alpha)
{
    const auto channel{[alpha](std::uint8_t a, std::uint8_t b)
                       { return static_cast<std::uint8_t>((a * (255 - alpha) + b * alpha + 127) / 255); }};
    return {channel(from.red, to.red), channel(from.green, to.green), channel(from.blue, to.blue)};
}

[[nodiscard]] Color darker(Color color)
{
    return mix(color, {0, 0, 0}, 90);
}

} // namespace

GraphicalStyle classic_style()
{
    GraphicalStyle style;
    // What a window larger than a whole number of cells leaves over, at its right and bottom edges.
    style.desktop = style.face;
    return style;
}

std::vector<GraphicalTheme> graphical_themes()
{
    auto ocean{classic_style()};
    ocean.face = {168, 174, 182};
    ocean.light = {198, 204, 212};
    ocean.shadow = {124, 130, 140};
    ocean.title = {60, 92, 140};
    ocean.well = {28, 32, 38};
    ocean.well_text = {206, 212, 220};
    ocean.text = {14, 18, 24};
    ocean.muted_text = {80, 88, 100};
    ocean.selection = {226, 232, 240};
    ocean.selection_text = {14, 18, 24};
    ocean.desktop = ocean.face;

    auto night{classic_style()};
    night.face = {64, 62, 58};
    night.light = {90, 88, 82};
    night.shadow = {40, 38, 36};
    night.title = {112, 88, 52};
    night.title_text = {240, 232, 214};
    night.well = {26, 26, 24};
    night.well_text = {208, 208, 200};
    night.text = {226, 222, 210};
    night.muted_text = {150, 146, 136};
    night.selection = {112, 88, 52};
    night.selection_text = {240, 232, 214};
    night.desktop = night.face;

    return {{"classic", translate("GraphicalTheme", "Classic"), classic_style()},
            {"ocean", translate("GraphicalTheme", "Ocean"), ocean},
            {"night", translate("GraphicalTheme", "Night"), night}};
}

Theme classic_theme()
{
    return theme_for(classic_style());
}

Theme theme_for(const GraphicalStyle& style)
{
    const auto text{style.text};
    const auto dim_text{style.muted_text};
    Theme theme;
    theme.background = style.face;
    theme.text = text;
    theme.muted_text = dim_text;
    theme.control_background = style.face;
    theme.panel_background = style.well;
    theme.panel_foreground = style.well_text;
    theme.shadow = style.shadow;
    theme.focus_background = mix(style.well, style.well_text, 24);
    theme.focus_text = style.title_text;
    theme.dialog_background = style.face;
    theme.dialog_foreground = text;
    theme.dialog_border = style.light;
    theme.dialog_heading_foreground = style.title;
    theme.dialog_icon_foreground = text;
    theme.shadow_foreground = style.shadow;
    theme.list_background = style.face;
    theme.list_foreground = text;
    theme.cluster_background = style.face;
    theme.cluster_foreground = text;
    theme.cluster_focused_foreground = text;
    theme.cluster_shortcut_foreground = text;
    theme.dialog_border_placing = style.focus;
    theme.table_header_background = style.face;
    theme.table_header_foreground = text;
    theme.table_background = style.well;
    theme.table_foreground = style.well_text;
    theme.menu_background = style.face;
    theme.menu_foreground = text;
    theme.menu_border = style.shadow;
    theme.menu_selected_background = style.title;
    theme.menu_selected_foreground = style.title_text;
    theme.status_background = style.face;
    theme.status_foreground = text;
    theme.button_background = style.face;
    theme.button_foreground = text;
    theme.button_default_foreground = text;
    theme.button_shortcut_foreground = text;
    theme.label_shortcut_foreground = text;
    theme.button_focused_background = style.face;
    theme.button_focused_foreground = text;
    theme.button_pressed_background = style.face;
    theme.button_pressed_foreground = text;
    theme.tab_active_background = style.face;
    theme.tab_active_foreground = text;
    theme.tab_inactive_background = style.face;
    theme.tab_inactive_foreground = dim_text;
    theme.selection_background = style.selection;
    theme.selection_foreground = style.selection_text;
    theme.item_selected_background = style.selection;
    theme.item_selected_foreground = style.selection_text;
    theme.search_match_background = {150, 110, 40};
    theme.search_match_foreground = {255, 255, 255};
    theme.occurrence_background = {70, 80, 100};
    theme.occurrence_foreground = {255, 255, 255};
    theme.bracket_match_background = {70, 110, 60};
    theme.bracket_match_foreground = {255, 255, 255};
    // Comparing, on the editor's dark well: red and green deep enough for its light text.
    theme.diff_removed_background = {120, 40, 40};
    theme.diff_removed_foreground = {255, 255, 255};
    theme.diff_added_background = {40, 105, 50};
    theme.diff_added_foreground = {255, 255, 255};
    theme.diff_changed_background = {50, 62, 86};
    theme.diff_changed_foreground = {255, 255, 255};
    theme.diff_filler_background = {24, 24, 22};
    theme.scrollbar_background = style.shadow;
    theme.scrollbar_foreground = style.face;
    theme.editor_background = style.well;
    theme.editor_foreground = style.well_text;
    theme.reading_background = style.well;
    theme.reading_foreground = style.well_text;
    theme.side_panel_background = style.well;
    theme.syntax_keyword = {232, 184, 72};
    theme.syntax_type = {120, 196, 208};
    theme.syntax_function = {226, 222, 206};
    theme.syntax_string = {148, 204, 112};
    theme.syntax_number = {220, 140, 100};
    theme.syntax_comment = {124, 124, 116};
    theme.syntax_preprocessor = {196, 150, 210};
    theme.syntax_operator = {180, 180, 170};
    theme.heading_foreground = {232, 184, 72};
    theme.code_foreground = {120, 196, 208};
    theme.code_background = mix(style.well, {0, 0, 0}, 80);
    theme.link_foreground = {148, 204, 112};
    theme.input_background = style.well;
    theme.input_foreground = style.well_text;
    theme.disabled_foreground = mix(style.face, text, 110);
    theme.line_number_foreground = theme.disabled_foreground;
    theme.hotkey_foreground = text;
    theme.error_foreground = {220, 80, 60};
    theme.warning_foreground = {232, 184, 72};
    theme.info_foreground = {120, 196, 208};
    return theme;
}

GraphicalPainter::GraphicalPainter(Canvas& canvas, const GraphicalStyle& style, Font& cell_font, Font& interface_font,
                                   Font& symbols, std::optional<PixelRect> area)
    : m_canvas{canvas}, m_style{style}, m_cell_font{cell_font}, m_interface_font{interface_font},
      m_fonts{cell_font, interface_font, symbols}, m_cell{cell_size(cell_font)},
      m_area{area.value_or(PixelRect{0, 0, canvas.width(), canvas.height()})}
{
    m_canvas.set_clip(m_area);
}

Size GraphicalPainter::cell_size(Font& cell_font)
{
    return {cell_font.glyph(U'M').advance, cell_font.line_height()};
}

int GraphicalPainter::row_offset(int row, int height, int cell_height)
{
    const auto rows{height / cell_height};
    if(rows <= 0)
    {
        return row * cell_height;
    }
    const auto left_over{height - rows * cell_height};
    return row * cell_height + std::min(row, rows) * left_over / rows;
}

int GraphicalPainter::row_at(int y, int height, int cell_height)
{
    // Rows are a cell tall or more, so the row is the one a cell's height gives or one before it.
    auto row{y / cell_height};
    while(row > 0 && row_offset(row, height, cell_height) > y)
    {
        -- row;
    }
    return row;
}

PixelRect GraphicalPainter::pixels(const Rect& cells) const
{
    const auto top{row_offset(cells.y, m_area.height, m_cell.height)};
    const auto bottom{row_offset(cells.y + cells.height, m_area.height, m_cell.height)};
    return {m_area.x + cells.x * m_cell.width, m_area.y + top, cells.width * m_cell.width, bottom - top};
}

void GraphicalPainter::fill_rect(const Rect& rect, Color color)
{
    m_canvas.fill(pixels(rect), color);
}

void GraphicalPainter::draw_text(Point position, std::string_view text, const TextStyle& style)
{
    struct Placed
    {
        int column;
        int width;
        char32_t glyph;
    };
    // Combining accents merge into the character before them, as the terminal's cells do.
    std::vector<Placed> placed;
    auto x{position.x};
    for(std::size_t index{}; index < text.size();)
    {
        const char32_t glyph{decode_utf8(text, index)};
        const auto width{terminal_cell_width(glyph)};
        if(width == 0 && !placed.empty() && is_diacritic(glyph))
        {
            if(const auto composed{compose(placed.back().glyph, glyph)})
            {
                placed.back().glyph = *composed;
            }
            continue;
        }
        if(width > 0)
        {
            placed.push_back({x, width, glyph});
        }
        x += width;
    }
    const auto foreground{(style.attributes & text_dim) != 0 ? mix(style.foreground, style.background, 110)
                                                             : style.foreground};
    for(const auto& cell : placed)
    {
        const auto area{pixels({cell.column, position.y, cell.width, 1})};
        m_canvas.fill(area, style.background);
        if(cell.glyph == U' ')
        {
            continue;
        }
        const auto& shape{cell_glyph(cell.glyph)};
        const auto pen{area.x + (area.width - shape.advance) / 2};
        const auto baseline{area.y + m_cell_font.ascent()};
        m_canvas.draw_glyph(shape, pen, baseline, foreground);
        if((style.attributes & text_bold) != 0)
        {
            m_canvas.draw_glyph(shape, pen + 1, baseline, foreground);
        }
        if((style.attributes & text_underline) != 0)
        {
            m_canvas.fill({area.x, baseline + 2, area.width, 1}, foreground);
        }
    }
}

void GraphicalPainter::push_clip(const Rect& rect)
{
    m_clips.push_back(m_canvas.clip());
    m_canvas.set_clip(intersect(m_canvas.clip(), pixels(rect)));
}

void GraphicalPainter::pop_clip()
{
    if(m_clips.empty())
    {
        throw std::logic_error{"Cannot pop an empty clip stack."};
    }
    m_canvas.set_clip(m_clips.back());
    m_clips.pop_back();
}

void GraphicalPainter::edges(const PixelRect& rect, Color top_left, Color bottom_right)
{
    const auto line{m_style.line};
    m_canvas.fill({rect.x, rect.y, rect.width, line}, top_left);
    m_canvas.fill({rect.x, rect.y, line, rect.height}, top_left);
    m_canvas.fill({rect.x, rect.y + rect.height - line, rect.width, line}, bottom_right);
    m_canvas.fill({rect.x + rect.width - line, rect.y, line, rect.height}, bottom_right);
}

void GraphicalPainter::bevel(const PixelRect& rect, bool sunken, Color face)
{
    m_canvas.fill(rect, face);
    edges(rect, sunken ? m_style.shadow : m_style.light, sunken ? m_style.light : m_style.shadow);
}

void GraphicalPainter::focus_ring(const PixelRect& rect)
{
    m_canvas.fill({rect.x, rect.y, rect.width, 1}, m_style.focus);
    m_canvas.fill({rect.x, rect.y + rect.height - 1, rect.width, 1}, m_style.focus);
    m_canvas.fill({rect.x, rect.y, 1, rect.height}, m_style.focus);
    m_canvas.fill({rect.x + rect.width - 1, rect.y, 1, rect.height}, m_style.focus);
}

const GlyphBitmap& GraphicalPainter::cell_glyph(char32_t character)
{
    return m_fonts.cell_glyph(character);
}

const GlyphBitmap& GraphicalPainter::interface_glyph(char32_t character)
{
    return m_fonts.interface_glyph(character);
}

int GraphicalPainter::interface_width(std::string_view text)
{
    return m_fonts.interface_width(text);
}

int GraphicalPainter::label(const PixelRect& area, std::string_view text, Color color, Align align, bool bold)
{
    const auto width{interface_width(text)};
    const auto x{align == Align::center ? area.x + (area.width - width) / 2
                 : align == Align::end  ? area.x + area.width - width
                                        : area.x};
    const auto top{area.y + (area.height - m_interface_font.line_height()) / 2};
    const auto baseline{top + m_interface_font.ascent()};
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip(intersect(previous, area));
    auto pen{x};
    for(std::size_t index{}; index < text.size();)
    {
        const auto& shape{interface_glyph(decode_utf8(text, index))};
        m_canvas.draw_glyph(shape, pen, baseline, color);
        if(bold)
        {
            m_canvas.draw_glyph(shape, pen + 1, baseline, color);
        }
        pen += shape.advance;
    }
    m_canvas.set_clip(previous);
    return x;
}

void GraphicalPainter::draw_label(Point position, std::string_view text, const TextStyle& style, Align align)
{
    const auto area{pixels({position.x, position.y, shown_width(text), 1})};
    m_canvas.fill(area, style.background);
    const auto foreground{(style.attributes & text_dim) != 0 ? mix(style.foreground, style.background, 110)
                                                             : style.foreground};
    const auto x{label(with_margin(area, align), text, foreground, align, (style.attributes & text_bold) != 0)};
    if((style.attributes & text_underline) != 0)
    {
        const auto top{area.y + (area.height - m_interface_font.line_height()) / 2};
        m_canvas.fill({x, top + m_interface_font.ascent() + 2, interface_width(text), 1}, foreground);
    }
}

void GraphicalPainter::draw_label_runs(Point position, const std::vector<LabelRun>& runs, Color background)
{
    int columns{};
    for(const auto& run : runs)
    {
        columns += text_width(run.text);
    }
    const auto area{pixels({position.x, position.y, columns, 1})};
    m_canvas.fill(area, background);
    const auto top{area.y + (area.height - m_interface_font.line_height()) / 2};
    auto pen{area.x};
    for(const auto& run : runs)
    {
        const auto width{interface_width(run.text)};
        // A run with a background of its own, as code within a sentence, shows it round its letters only.
        if(run.style.background != background)
        {
            m_canvas.fill({pen, area.y + 2, width, area.height - 4}, run.style.background);
        }
        const auto foreground{(run.style.attributes & text_dim) != 0
                                  ? mix(run.style.foreground, run.style.background, 110)
                                  : run.style.foreground};
        label({pen, area.y, width, area.height}, run.text, foreground, Align::start,
              (run.style.attributes & text_bold) != 0);
        if((run.style.attributes & text_underline) != 0)
        {
            m_canvas.fill({pen, top + m_interface_font.ascent() + 2, width, 1}, foreground);
        }
        pen += width;
    }
}

void GraphicalPainter::draw_mnemonic_label(Point position, std::string_view text, std::size_t mnemonic,
                                           const TextStyle& style, Color mnemonic_color, Align align)
{
    const auto area{pixels({position.x, position.y, shown_width(text), 1})};
    m_canvas.fill(area, style.background);
    const auto x{label(with_margin(area, align), text, style.foreground, align, (style.attributes & text_bold) != 0)};
    auto end{mnemonic};
    static_cast<void>(decode_utf8(text, end));
    const auto top{area.y + (area.height - m_interface_font.line_height()) / 2};
    m_canvas.fill({x + interface_width(text.substr(0, mnemonic)), top + m_interface_font.ascent() + 2,
                   interface_width(text.substr(mnemonic, end - mnemonic)), 1},
                  mnemonic_color);
}

void GraphicalPainter::draw_window(const Rect& bounds, const WindowLook& look, const Theme&)
{
    const auto area{pixels(bounds)};
    bevel(area, false, look.background);
    // The title bar fills the top row, inside the edges.
    const PixelRect bar{area.x + m_style.line + 1, area.y + m_style.line + 1, area.width - 2 * (m_style.line + 1),
                        m_cell.height - m_style.line - 1};
    const auto bar_color{look.placing ? m_style.focus : m_style.title};
    m_canvas.fill(bar, bar_color);
    m_canvas.fill({bar.x, bar.y + bar.height - 1, bar.width, 1}, darker(bar_color));
    // The close button sits at the right of the bar, in the window's last two cells and its edge; maximize and
    // minimize, when there are, in the two cells before each.
    const auto button_size{bar.height - 4};
    const PixelRect close{bar.x + bar.width - button_size - 2, bar.y + 2, button_size, button_size};
    const auto before{[&close, this](int slots)
                      { return PixelRect{close.x - 2 * slots * m_cell.width, close.y, close.width, close.height}; }};
    const auto maximize{before(1)};
    const auto minimize{before(look.maximizable ? 2 : 1)};
    auto first_button{close.x};
    if(look.maximizable)
    {
        first_button = maximize.x;
    }
    if(look.minimizable)
    {
        first_button = minimize.x;
    }
    const auto text_end{look.closable || look.maximizable || look.minimizable ? first_button - 4
                                                                              : bar.x + bar.width - 6};
    const PixelRect text_area{bar.x + 6, bar.y, text_end - bar.x - 6, bar.height};
    const auto light{mix(bar_color, {255, 255, 255}, 70)};
    const auto dark{darker(bar_color)};
    const auto face{[&](const PixelRect& button, bool pressed)
                    {
                        m_canvas.fill(button, bar_color);
                        edges(button, pressed ? dark : light, pressed ? light : dark);
                    }};
    const auto stroke{1.6f * unit()};
    const auto centre_x{[](const PixelRect& button)
                        { return static_cast<float>(button.x) + static_cast<float>(button.width) / 2; }};
    const auto centre_y{[](const PixelRect& button)
                        { return static_cast<float>(button.y) + static_cast<float>(button.height) / 2; }};
    const auto half{static_cast<float>(button_size) / 5};
    if(look.closable)
    {
        face(close, look.close_pressed);
        const auto shift{look.close_pressed ? 1.0f : 0.0f};
        const auto cx{centre_x(close) + shift};
        const auto cy{centre_y(close) + shift};
        m_canvas.draw_line({cx - half, cy - half}, {cx + half, cy + half}, stroke, m_style.title_text);
        m_canvas.draw_line({cx + half, cy - half}, {cx - half, cy + half}, stroke, m_style.title_text);
    }
    // A square outline with a thicker top, as a window.
    const auto outline{[&](PixelPoint from, PixelPoint to)
                       {
                           m_canvas.draw_line({from.x, from.y + 1}, {to.x, from.y + 1}, 2.0f, m_style.title_text);
                           m_canvas.draw_line({from.x, to.y}, {to.x, to.y}, 1.2f, m_style.title_text);
                           m_canvas.draw_line({from.x, from.y}, {from.x, to.y}, 1.2f, m_style.title_text);
                           m_canvas.draw_line({to.x, from.y}, {to.x, to.y}, 1.2f, m_style.title_text);
                       }};
    if(look.maximizable)
    {
        face(maximize, false);
        const auto cx{centre_x(maximize)};
        const auto cy{centre_y(maximize)};
        const auto size{half * 1.6f};
        if(look.maximized)
        {
            // Put back: a window behind another.
            outline({cx - size + 2.5f, cy - size}, {cx + size, cy + size - 2.5f});
            m_canvas.fill({static_cast<int>(cx - size), static_cast<int>(cy - size + 2.5f),
                           static_cast<int>(2 * size - 2.5f), static_cast<int>(2 * size - 2.5f)},
                          bar_color);
            outline({cx - size, cy - size + 2.5f}, {cx + size - 2.5f, cy + size});
        }
        else
        {
            outline({cx - size, cy - size}, {cx + size, cy + size});
        }
    }
    if(look.minimizable)
    {
        face(minimize, false);
        const auto cx{centre_x(minimize)};
        const auto cy{centre_y(minimize)};
        const auto size{half * 1.6f};
        m_canvas.draw_line({cx - size, cy + size - 1}, {cx + size, cy + size - 1}, 2.0f, m_style.title_text);
    }
    label({text_area.x + 1, text_area.y + 1, text_area.width, text_area.height}, look.title, darker(bar_color),
          Align::start);
    label(text_area, look.title, m_style.title_text, Align::start);
    if(look.resizable)
    {
        // Three short diagonals in the corner, as a grip.
        const auto right{static_cast<float>(area.x + area.width - m_style.line - 2)};
        const auto bottom{static_cast<float>(area.y + area.height - m_style.line - 2)};
        for(const auto length : {4 * unit(), 8 * unit(), 12 * unit()})
        {
            m_canvas.draw_line({right - length, bottom}, {right, bottom - length}, 1.2f, m_style.shadow);
        }
    }
}

void GraphicalPainter::draw_button(const Rect& bounds, const ButtonLook& look, const Theme& theme)
{
    const auto area{pixels(bounds)};
    m_canvas.fill(area, look.around);
    // A terminal button keeps a column and a row for its shadow; here, with no shadow drawn, the button takes its
    // whole width, as the fields beside it do, and the middle of its rows.
    auto shape{area};
    if(look.compact || bounds.height < 2)
    {
        shape.y += 1;
        shape.height = m_cell.height - 2;
    }
    else
    {
        const auto height{m_cell.height * 17 / 12};
        shape.y += (2 * m_cell.height - height) / 2;
        shape.height = height;
    }
    bevel(shape, look.pressed, theme.button_background);
    const auto shift{look.pressed ? 1 : 0};
    label({shape.x + shift, shape.y + shift, shape.width, shape.height}, look.text,
          look.enabled ? theme.button_foreground : theme.disabled_foreground, Align::center);
    if(look.focused)
    {
        focus_ring({shape.x + 3, shape.y + 3, shape.width - 6, shape.height - 6});
    }
}

void GraphicalPainter::draw_check_box(const Rect& bounds, const CheckBoxLook& look, const Theme& theme)
{
    const auto area{pixels(bounds)};
    m_canvas.fill(area, look.background);
    const auto size{m_cell.height * 2 / 3};
    const PixelRect box{area.x + 2, area.y + (m_cell.height - size) / 2, size, size};
    bevel(box, true, m_style.light);
    if(look.checked)
    {
        m_canvas.fill({box.x + 4, box.y + 4, size - 8, size - 8}, theme.dialog_foreground);
    }
    const auto text_x{box.x + size + 8};
    label({text_x, area.y, area.x + area.width - text_x, m_cell.height}, look.text,
          look.enabled ? theme.dialog_foreground : theme.disabled_foreground, Align::start);
    if(look.focused)
    {
        const auto right{std::min(area.x + area.width, text_x + interface_width(look.text) + 4)};
        focus_ring({area.x, area.y + 1, right - area.x, m_cell.height - 2});
    }
}

void GraphicalPainter::draw_tab(const Rect& bounds, const TabLook& look, const Theme& theme)
{
    const auto area{pixels(bounds)};
    m_canvas.fill(area, look.selected ? theme.tab_active_background : theme.tab_inactive_background);
    if(look.selected)
    {
        // Raised, open at the bottom into what it shows.
        const auto line{m_style.line};
        m_canvas.fill({area.x, area.y, area.width, line}, m_style.light);
        m_canvas.fill({area.x, area.y, line, area.height}, m_style.light);
        m_canvas.fill({area.x + area.width - line, area.y, line, area.height}, m_style.shadow);
    }
    const auto title_columns{shown_width(look.title)};
    label({area.x + m_cell.width, area.y, title_columns * m_cell.width, m_cell.height}, look.title,
          look.selected ? theme.tab_active_foreground : theme.tab_inactive_foreground, Align::center);
    if(!look.closable)
    {
        return;
    }
    // The close mark sits in the cell a click closes; a document with unsaved changes shows a dot there instead.
    const auto cell{pixels({bounds.x + title_columns + 2, bounds.y, 1, 1})};
    const auto cx{static_cast<float>(cell.x) + static_cast<float>(cell.width) / 2};
    const auto cy{static_cast<float>(cell.y) + static_cast<float>(cell.height) / 2};
    const auto ink{look.selected ? theme.tab_active_foreground : theme.tab_inactive_foreground};
    if(look.highlighted)
    {
        std::vector<PixelPoint> dot;
        for(int step{}; step < 16; ++ step)
        {
            const auto angle{static_cast<float>(step) * 3.14159265f / 8};
            dot.push_back({cx + 3.5f * unit() * std::cos(angle), cy + 3.5f * unit() * std::sin(angle)});
        }
        m_canvas.fill_polygon(dot, ink);
        return;
    }
    const auto half{3.5f * unit()};
    m_canvas.draw_line({cx - half, cy - half}, {cx + half, cy + half}, 1.5f, ink);
    m_canvas.draw_line({cx + half, cy - half}, {cx - half, cy + half}, 1.5f, ink);
}

void GraphicalPainter::draw_divider(const Rect& line, const Theme&)
{
    const auto area{pixels(line)};
    m_canvas.fill(area, m_style.face);
    edges(area, m_style.light, m_style.shadow);
}

void GraphicalPainter::draw_scrollbar(const Rect& track, const ScrollbarLook& look, const Theme&)
{
    const auto area{pixels(track)};
    bevel(area, true, m_style.shadow);
    if(track.width == 1)
    {
        const auto thumb{pixels({track.x, look.thumb_start, 1, look.thumb_length})};
        bevel({area.x, thumb.y, area.width, thumb.height}, false, m_style.face);
        return;
    }
    // Across, a row is twice as tall as a column is wide: the bar takes the middle half of it.
    const PixelRect bar{area.x, area.y + area.height / 4, area.width, area.height / 2};
    bevel(bar, true, m_style.shadow);
    const auto thumb{pixels({look.thumb_start, track.y, look.thumb_length, 1})};
    bevel({thumb.x, bar.y, thumb.width, bar.height}, false, m_style.face);
}

void GraphicalPainter::draw_bar(const Rect& bounds, BarKind kind, const Theme&)
{
    const auto area{pixels(bounds)};
    m_canvas.fill(area, m_style.face);
    if(kind == BarKind::menu)
    {
        m_canvas.fill({area.x, area.y + area.height - m_style.line, area.width, m_style.line}, m_style.shadow);
    }
}

PixelRect GraphicalPainter::popup_area(const Rect& bounds) const
{
    // The entries fill the popup's cells; its background reaches a little past them, above over the bar it drops
    // from, so they keep some room from its edges.
    const auto margin{m_cell.height / 6};
    const auto area{pixels(bounds)};
    return {area.x, area.y - margin, area.width, area.height + 2 * margin};
}

void GraphicalPainter::draw_popup(const Rect& bounds, const Theme& theme)
{
    m_canvas.fill(popup_area(bounds), theme.menu_background);
}

void GraphicalPainter::draw_popup_edge(const Rect& bounds, const Theme&)
{
    edges(popup_area(bounds), m_style.light, m_style.shadow);
}

void GraphicalPainter::draw_popup_separator(const Rect& row, const Theme&)
{
    const auto area{pixels(row)};
    const auto y{area.y + area.height / 2 - 1};
    m_canvas.fill({area.x + m_cell.width / 2, y, area.width - m_cell.width, 1}, m_style.shadow);
    m_canvas.fill({area.x + m_cell.width / 2, y + 1, area.width - m_cell.width, 1}, m_style.light);
}

void GraphicalPainter::draw_header(const Rect& cell, const HeaderLook& look, const Theme& theme)
{
    // Raised as a button is, and sunken while held down; the column sorted by has its name in bold and an arrow at
    // its right.
    const auto area{pixels(cell)};
    bevel(area, look.pressed, theme.button_background);
    const auto shift{look.pressed ? 1 : 0};
    const auto u{unit()};
    // A little more room than plain text keeps, inside the raised edge.
    const auto margin{m_style.text_margin + static_cast<int>(4 * u)};
    const auto arrow_room{look.sort ? static_cast<int>(14 * u) : 0};
    label({area.x + margin + shift, area.y + shift, std::max(0, area.width - 2 * margin - arrow_room), area.height},
          look.text, theme.button_foreground, Align::start, look.sort.has_value());
    if(!look.sort)
    {
        return;
    }
    const auto cx{static_cast<float>(area.x + area.width - margin + shift) - 5 * u};
    const auto cy{static_cast<float>(area.y + shift) + static_cast<float>(area.height) / 2};
    if(*look.sort == Icon::sorted_up)
    {
        m_canvas.fill_polygon({{cx - 5 * u, cy + 3 * u}, {cx + 5 * u, cy + 3 * u}, {cx, cy - 2.5f * u}},
                              theme.button_foreground);
    }
    else
    {
        m_canvas.fill_polygon({{cx - 5 * u, cy - 2.5f * u}, {cx + 5 * u, cy - 2.5f * u}, {cx, cy + 3 * u}},
                              theme.button_foreground);
    }
}

void GraphicalPainter::draw_icon(Point position, Icon icon, const TextStyle& style)
{
    const auto cell{pixels({position.x, position.y, 1, 1})};
    m_canvas.fill(cell, style.background);
    const auto cx{static_cast<float>(cell.x) + static_cast<float>(cell.width) / 2};
    const auto cy{static_cast<float>(cell.y) + static_cast<float>(cell.height) / 2};
    const auto u{unit()};
    switch(icon)
    {
    case Icon::submenu:
        m_canvas.fill_polygon({{cx - 2.5f * u, cy - 5 * u}, {cx + 3 * u, cy}, {cx - 2.5f * u, cy + 5 * u}},
                              style.foreground);
        break;
    case Icon::drop_down:
    case Icon::sorted_down:
        m_canvas.fill_polygon({{cx - 5 * u, cy - 2.5f * u}, {cx + 5 * u, cy - 2.5f * u}, {cx, cy + 3 * u}},
                              style.foreground);
        break;
    case Icon::sorted_up:
        m_canvas.fill_polygon({{cx - 5 * u, cy + 3 * u}, {cx + 5 * u, cy + 3 * u}, {cx, cy - 2.5f * u}},
                              style.foreground);
        break;
    case Icon::task_open:
    case Icon::task_done:
        draw_marker(position, {{}, 1, icon}, style);
        break;
    case Icon::folder:
    case Icon::file:
    case Icon::parent_folder:
        draw_file_icon(icon, {static_cast<float>(cell.x) - 2 * u, cy - 7 * u}, 14 * u);
        break;
    }
}

void GraphicalPainter::draw_check_mark(Point position, bool checked, const TextStyle& style)
{
    const auto area{pixels({position.x, position.y, 4, 1})};
    m_canvas.fill(area, style.background);
    // A small sunken box, as the check box's, with a square in it when checked.
    const auto size{m_cell.height * 7 / 12};
    const PixelRect box{area.x + 2, area.y + (area.height - size) / 2, size, size};
    bevel(box, true, m_style.light);
    if(checked)
    {
        m_canvas.fill({box.x + 4, box.y + 4, size - 8, size - 8}, m_style.text);
    }
}

void GraphicalPainter::draw_tooltip(const Rect& bounds, const std::vector<std::string>& lines, const Theme&)
{
    // Light, over what it covers, which shows faintly through it, inside a dark edge.
    const auto area{pixels(bounds)};
    constexpr int alpha{195};
    m_canvas.fill_blended({area.x + 1, area.y + 1, area.width - 2, area.height - 2}, m_style.light, alpha);
    m_canvas.fill({area.x, area.y, area.width, 1}, m_style.text);
    m_canvas.fill({area.x, area.y + area.height - 1, area.width, 1}, m_style.text);
    m_canvas.fill({area.x, area.y + 1, 1, area.height - 2}, m_style.text);
    m_canvas.fill({area.x + area.width - 1, area.y + 1, 1, area.height - 2}, m_style.text);
    for(int row{}; row < bounds.height && row < static_cast<int>(lines.size()); ++ row)
    {
        // As far in from the edge as a terminal's space is.
        const auto line{pixels({bounds.x, bounds.y + row, bounds.width, 1})};
        label(
            {line.x + m_style.text_margin * 2, line.y, std::max(0, line.width - m_style.text_margin * 3), line.height},
            lines[static_cast<std::size_t>(row)], m_style.text, Align::start);
    }
}

void GraphicalPainter::draw_drag_image(Point position, int cell_offset, std::string_view text, const Theme& theme)
{
    // A see-through tab, centred on the pointer, over everything.
    const auto cell{pixels({position.x, position.y, 1, 1})};
    const auto width{interface_width(text) + 4 * m_style.text_margin};
    const PixelRect tab{cell.x + cell_offset - width / 2, cell.y - cell.height / 3, width, cell.height};
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip({0, 0, m_canvas.width(), m_canvas.height()});
    constexpr int alpha{170};
    m_canvas.fill_blended(tab, theme.tab_active_background, alpha);
    m_canvas.fill_blended({tab.x, tab.y, tab.width, 1}, m_style.light, alpha);
    m_canvas.fill_blended({tab.x, tab.y + 1, 1, tab.height - 1}, m_style.light, alpha);
    m_canvas.fill_blended({tab.x + tab.width - 1, tab.y + 1, 1, tab.height - 1}, m_style.shadow, alpha);
    m_canvas.fill_blended({tab.x + 1, tab.y + tab.height - 1, tab.width - 2, 1}, m_style.shadow, alpha);
    auto pen{tab.x + 2 * m_style.text_margin};
    const auto baseline{tab.y + (tab.height - m_interface_font.line_height()) / 2 + m_interface_font.ascent()};
    for(std::size_t index{}; index < text.size();)
    {
        const auto& shape{interface_glyph(decode_utf8(text, index))};
        m_canvas.draw_glyph(shape, pen, baseline, theme.tab_active_foreground, 220);
        pen += shape.advance;
    }
    m_canvas.set_clip(previous);
}

void GraphicalPainter::draw_text_field(const Rect& bounds, const TextFieldLook& look, const Theme&)
{
    const auto area{pixels(bounds)};
    m_canvas.fill(area, look.style.background);
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip(intersect(previous, area));
    const auto start{area.x + field_padding(m_cell)};
    const auto x_at{[&](std::size_t byte) { return start + interface_width(look.text.substr(0, byte)); }};
    const auto first{std::min(look.selection_start, look.text.size())};
    const auto last{std::min(std::max(look.selection_end, first), look.text.size())};
    if(first < last)
    {
        m_canvas.fill({x_at(first), area.y + 2, x_at(last) - x_at(first), area.height - 4}, look.selected.background);
    }
    // Before, in and after the selection, each in its colour.
    const auto run{[&](std::size_t from, std::size_t to, Color color)
                   {
                       if(from < to)
                       {
                           label({x_at(from), area.y, area.x + area.width - x_at(from), area.height},
                                 look.text.substr(from, to - from), color, Align::start);
                       }
                   }};
    run(0, first, look.style.foreground);
    run(first, last, look.selected.foreground);
    run(last, look.text.size(), look.style.foreground);
    if(look.cursor)
    {
        // A bar between the characters.
        m_canvas.fill({x_at(std::min(*look.cursor, look.text.size())) - 1, area.y + 3, 2, area.height - 6},
                      look.style.foreground);
    }
    m_canvas.set_clip(previous);
}

void GraphicalPainter::draw_image(const Rect& bounds, const PixelImage& image, const Theme&)
{
    const auto area{pixels(bounds)};
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip(intersect(previous, area));
    m_canvas.copy(area.x, area.y, image.pixels, image.width, image.height, image.stride);
    m_canvas.set_clip(previous);
}

int GraphicalPainter::field_padding(Size cell)
{
    return cell.width / 2;
}

float GraphicalPainter::unit() const
{
    // Sizes are drawn for cells 24 pixels high and grow with them.
    return static_cast<float>(m_cell.height) / 24.0f;
}

PixelRect GraphicalPainter::with_margin(const PixelRect& area, Align align) const
{
    switch(align)
    {
    case Align::start:
        return {area.x + m_style.text_margin, area.y, area.width - m_style.text_margin, area.height};
    case Align::end:
        return {area.x, area.y, area.width - m_style.text_margin, area.height};
    case Align::center:
        break;
    }
    return area;
}

void GraphicalPainter::draw_marker(Point position, const Marker& marker, const TextStyle& style)
{
    const auto area{pixels({position.x, position.y, marker.columns, 1})};
    m_canvas.fill(area, style.background);
    if(!marker.icon)
    {
        draw_text(position, marker.text, style);
        return;
    }
    if(*marker.icon == Icon::task_open || *marker.icon == Icon::task_done)
    {
        // A box as a check box's, with a square in it once done.
        const auto box_size{m_cell.height * 7 / 12};
        const PixelRect box{area.x + m_style.text_margin, area.y + (area.height - box_size) / 2, box_size, box_size};
        bevel(box, true, m_style.light);
        if(*marker.icon == Icon::task_done)
        {
            const auto inset{box_size / 4};
            m_canvas.fill({box.x + inset, box.y + inset, box_size - 2 * inset, box_size - 2 * inset}, m_style.text);
        }
        return;
    }
    const auto size{16.0f * unit()};
    const auto icon_x{area.x + m_style.text_margin};
    draw_file_icon(
        *marker.icon,
        {static_cast<float>(icon_x), static_cast<float>(area.y) + (static_cast<float>(area.height) - size) / 2}, size);
    // The way up says so beside its icon, as the terminal's mark does, lined up with the names.
    if(*marker.icon == Icon::parent_folder)
    {
        // Where the names of the other entries start, after the mark.
        const auto text_x{area.x + area.width + m_style.text_margin};
        label({text_x, area.y, area.x + area.width + m_cell.width * 2 - text_x, area.height}, "..", style.foreground,
              Align::start);
    }
}

void GraphicalPainter::draw_file_icon(Icon icon, PixelPoint at, float size)
{
    const auto point{[&](float x, float y) { return PixelPoint{at.x + x * size / 16, at.y + y * size / 16}; }};
    const auto ink{mix(m_style.folder, {0, 0, 0}, 120)};
    switch(icon)
    {
    case Icon::folder:
    case Icon::parent_folder:
    {
        // A folder: its tab, then its body, each a dark shape with a lighter one inside.
        m_canvas.fill_polygon({point(0, 2), point(6, 2), point(8, 4), point(16, 4), point(16, 15), point(0, 15)}, ink);
        m_canvas.fill_polygon({point(1, 3), point(5.5f, 3), point(7.5f, 5), point(15, 5), point(15, 14), point(1, 14)},
                              m_style.folder);
        m_canvas.fill_polygon({point(1, 7), point(15, 7), point(15, 7.8f), point(1, 7.8f)}, ink);
        if(icon == Icon::parent_folder)
        {
            // An arrow up out of it.
            m_canvas.fill_polygon({point(8, 7.5f), point(12, 11), point(4, 11)}, ink);
            m_canvas.fill_polygon({point(7, 10.5f), point(9, 10.5f), point(9, 13.5f), point(7, 13.5f)}, ink);
        }
        break;
    }
    case Icon::file:
    {
        const auto edge{mix(m_style.page, {0, 0, 0}, 130)};
        m_canvas.fill_polygon({point(2, 0), point(10, 0), point(14, 4), point(14, 16), point(2, 16)}, edge);
        m_canvas.fill_polygon({point(3, 1), point(9.5f, 1), point(13, 4.5f), point(13, 15), point(3, 15)},
                              m_style.page);
        m_canvas.fill_polygon({point(9.5f, 1), point(9.5f, 4.5f), point(13, 4.5f)}, edge);
        for(const auto y : {7.0f, 9.5f, 12.0f})
        {
            m_canvas.fill_polygon({point(5, y), point(11, y), point(11, y + 0.8f), point(5, y + 0.8f)}, edge);
        }
        break;
    }
    default:
        break;
    }
}

void GraphicalPainter::draw_text_cursor(Point position, std::string_view, const TextStyle& style)
{
    // Two pixels wide at cells 24 high, as wide again for every 24 more, a pixel short of the cell above and below.
    const auto area{pixels({position.x, position.y, 1, 1})};
    const auto width{std::max(2, static_cast<int>(std::lround(2.0f * unit())))};
    m_canvas.fill({area.x, area.y + 1, width, area.height - 2}, style.foreground);
}

void GraphicalPainter::draw_progress(const Rect& bounds, const ProgressLook& look, const Theme&)
{
    // A sunken track along the middle of the row, filled in the title's colour as far as is done, and the text after
    // it in the interface font.
    const auto area{pixels(bounds)};
    const auto words{look.text.empty() ? 0 : (interface_width(look.text) + m_cell.width - 1) / m_cell.width + 1};
    const auto length{std::max(0, bounds.width - words) * m_cell.width};
    const PixelRect track{area.x, area.y + area.height / 5, length, area.height - 2 * (area.height / 5)};
    bevel(track, true, m_style.well);
    const auto inside{PixelRect{track.x + m_style.line, track.y + m_style.line, track.width - 2 * m_style.line,
                                track.height - 2 * m_style.line}};
    const auto done{static_cast<int>(std::lround(std::clamp(look.done, 0.0, 1.0) * inside.width))};
    m_canvas.fill({inside.x, inside.y, done, inside.height}, m_style.title);
    if(words > 0)
    {
        static_cast<void>(label({area.x + length + m_cell.width, area.y, area.width - length - m_cell.width, area.height},
                                look.text, m_style.text, Align::start));
    }
}

void GraphicalPainter::draw_field_frame(const Rect& bounds, bool focused, const Theme&)
{
    const auto area{pixels(bounds)};
    if(focused)
    {
        focus_ring(area);
        return;
    }
    edges(area, m_style.shadow, m_style.light);
}

} // namespace olagarro::oui
