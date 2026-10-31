#include <oui/graphics/graphical_painter.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>
#include <oui/trace.h>
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
    // Code in the warm colours of its surfaces, greens beside its green title.
    style.syntax = {{244, 200, 100}, {140, 206, 180}, {226, 222, 206}, {176, 214, 116},
                    {226, 150, 104}, {136, 136, 124}, {206, 160, 206}, {186, 186, 174}};
    return style;
}

std::vector<GraphicalTheme> graphical_themes()
{
    auto ocean{classic_style()};
    ocean.face = {168, 174, 182};
    ocean.light = {198, 204, 212};
    ocean.shadow = {124, 130, 140};
    ocean.title = {60, 92, 140};
    // The editor a deep sea blue, its code in cool colours.
    ocean.well = {16, 30, 52};
    ocean.well_text = {214, 224, 238};
    ocean.syntax = {{110, 168, 255}, {96, 206, 226},  {222, 230, 240}, {150, 210, 140},
                    {236, 160, 120}, {120, 138, 160}, {196, 160, 236}, {176, 190, 210}};
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
    // The editor nearly black, its text and code in warm, quiet colours, as by lamplight.
    night.well = {18, 17, 15};
    night.well_text = {214, 206, 188};
    night.syntax = {{226, 170, 90},  {170, 190, 140}, {226, 216, 196}, {190, 200, 110},
                    {220, 140, 110}, {130, 124, 112}, {200, 150, 170}, {180, 172, 156}};
    night.text = {226, 222, 210};
    night.muted_text = {150, 146, 136};
    night.selection = {112, 88, 52};
    night.selection_text = {240, 232, 214};
    night.desktop = night.face;

    // Light: warm paper surfaces, the editor a page with dark ink and its code in deep colours.
    auto paper{classic_style()};
    paper.face = {222, 216, 202};
    paper.light = {240, 236, 226};
    paper.shadow = {170, 162, 146};
    paper.title = {70, 110, 160};
    paper.title_text = {255, 255, 255};
    paper.well = {250, 247, 238};
    paper.well_text = {36, 36, 40};
    paper.syntax = {{20, 80, 170}, {0, 110, 120},   {36, 36, 40},   {40, 110, 30},
                    {160, 70, 10}, {110, 108, 100}, {120, 40, 140}, {70, 70, 74}};
    paper.text = {20, 20, 20};
    paper.muted_text = {96, 90, 80};
    paper.selection = {190, 210, 240};
    paper.selection_text = {20, 20, 24};
    paper.focus = {200, 120, 20};
    paper.desktop = paper.face;

    return {{"classic", translate("GraphicalTheme", "Classic"), classic_style()},
            {"ocean", translate("GraphicalTheme", "Ocean"), ocean},
            {"night", translate("GraphicalTheme", "Night"), night},
            {"dark", translate("GraphicalTheme", "Dark"), dark_style()},
            {"paper", translate("GraphicalTheme", "Paper"), paper}};
}

GraphicalStyle dark_style()
{
    // The accent: the title, the focus, the selection, and in a program what it marks as chosen.
    const Color accent{58, 124, 222};
    GraphicalStyle style;
    style.face = {45, 45, 48};
    style.light = {68, 68, 74};
    style.shadow = {24, 24, 26};
    style.title = accent;
    style.title_text = {255, 255, 255};
    style.well = {30, 30, 32};
    style.well_text = {220, 220, 224};
    style.focus = accent;
    style.text = {224, 224, 228};
    style.muted_text = {150, 150, 156};
    style.selection = accent;
    style.selection_text = {255, 255, 255};
    style.folder = {214, 178, 96};
    style.page = {208, 208, 212};
    style.desktop = style.face;
    style.flat = true;
    style.interface_font = InterfaceFont::noto_sans;
    return style;
}

Theme classic_theme()
{
    return theme_for(classic_style());
}

Theme dark_theme()
{
    return theme_for(dark_style());
}

void draw_icon(Canvas& canvas, Icon icon, const PixelRect& area, Color color)
{
    // On a grid of 16 by 16 in the middle of the area.
    const auto size{static_cast<float>(std::min(area.width, area.height))};
    const auto left{static_cast<float>(area.x) + (static_cast<float>(area.width) - size) / 2};
    const auto top{static_cast<float>(area.y) + (static_cast<float>(area.height) - size) / 2};
    const auto point{[&](float x, float y) { return PixelPoint{left + x * size / 16, top + y * size / 16}; }};
    const auto box{[&](float x0, float y0, float x1, float y1)
                   { canvas.fill_polygon({point(x0, y0), point(x1, y0), point(x1, y1), point(x0, y1)}, color); }};
    const auto stroke{size / 16 * 1.5f};
    // An arc about (x, y), from `from` to `to` radians, counted anticlockwise from the right as on paper.
    const auto arc{[&](float x, float y, float radius, float from, float to)
                   {
                       constexpr int steps{12};
                       for(int step{}; step < steps; ++ step)
                       {
                           const auto a{from + (to - from) * static_cast<float>(step) / steps};
                           const auto b{from + (to - from) * static_cast<float>(step + 1) / steps};
                           canvas.draw_line(point(x + radius * std::cos(a), y - radius * std::sin(a)),
                                            point(x + radius * std::cos(b), y - radius * std::sin(b)), stroke, color);
                       }
                   }};
    const auto disc{[&](float x, float y, float radius)
                    {
                        std::vector<PixelPoint> points;
                        constexpr int steps{16};
                        for(int step{}; step < steps; ++ step)
                        {
                            const auto a{6.2831853f * static_cast<float>(step) / steps};
                            points.push_back(point(x + radius * std::cos(a), y + radius * std::sin(a)));
                        }
                        canvas.fill_polygon(points, color);
                    }};
    constexpr float half_turn{3.14159265f};
    switch(icon)
    {
    case Icon::submenu:
        canvas.fill_polygon({point(5, 3), point(11, 8), point(5, 13)}, color);
        break;
    case Icon::drop_down:
    case Icon::sorted_down:
        canvas.fill_polygon({point(3, 5), point(13, 5), point(8, 11)}, color);
        break;
    case Icon::sorted_up:
        canvas.fill_polygon({point(3, 11), point(13, 11), point(8, 5)}, color);
        break;
    case Icon::folder:
    case Icon::parent_folder:
        canvas.fill_polygon({point(0, 2), point(6, 2), point(8, 4), point(16, 4), point(16, 15), point(0, 15)}, color);
        break;
    case Icon::file:
        canvas.fill_polygon({point(2, 0), point(10, 0), point(14, 4), point(14, 16), point(2, 16)}, color);
        break;
    case Icon::task_open:
        box(2, 2, 14, 3.5f);
        box(2, 12.5f, 14, 14);
        box(2, 2, 3.5f, 14);
        box(12.5f, 2, 14, 14);
        break;
    case Icon::task_done:
        box(2, 2, 14, 14);
        break;
    case Icon::play:
        canvas.fill_polygon({point(4, 2), point(14, 8), point(4, 14)}, color);
        break;
    case Icon::pause:
        box(3.5f, 2.5f, 6.5f, 13.5f);
        box(9.5f, 2.5f, 12.5f, 13.5f);
        break;
    case Icon::step_back:
        box(2.5f, 3, 4.5f, 13);
        canvas.fill_polygon({point(13.5f, 3), point(5.5f, 8), point(13.5f, 13)}, color);
        break;
    case Icon::step_forwards:
        box(11.5f, 3, 13.5f, 13);
        canvas.fill_polygon({point(2.5f, 3), point(10.5f, 8), point(2.5f, 13)}, color);
        break;
    case Icon::speaker:
        // A box and its cone, and two waves coming out.
        box(1.5f, 6, 4.5f, 10);
        canvas.fill_polygon({point(4, 6), point(8.5f, 2.5f), point(8.5f, 13.5f), point(4, 10)}, color);
        arc(8.5f, 8, 3, -0.8f, 0.8f);
        arc(8.5f, 8, 5.8f, -0.8f, 0.8f);
        break;
    case Icon::headphones:
        // A band over the head and a cup at either ear.
        arc(8, 9, 5.8f, 0, half_turn);
        box(1.5f, 9, 4.5f, 14.5f);
        box(11.5f, 9, 14.5f, 14.5f);
        break;
    case Icon::eye:
        // The lids, then the pupil.
        arc(8, 12.125f, 8.125f, 0.533f, half_turn - 0.533f);
        arc(8, 3.875f, 8.125f, -0.533f, 0.533f - half_turn);
        disc(8, 8, 2.6f);
        break;
    case Icon::lock:
        // The shackle over the body.
        arc(8, 7, 3.6f, 0, half_turn);
        box(3.65f, 7, 5.15f, 8);
        box(10.85f, 7, 12.35f, 8);
        box(2.5f, 8, 13.5f, 15);
        break;
    }
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
    // A light well, as paper's, has what is marked on it in pale colours under its dark text; a dark one in deep
    // colours under white.
    const auto light_well{style.well.red + style.well.green + style.well.blue > 3 * 128};
    const auto marked{[&](Color on_light, Color on_dark) { return light_well ? on_light : on_dark; }};
    const auto marked_text{marked(style.well_text, {255, 255, 255})};
    theme.search_match_background = marked({240, 200, 90}, {150, 110, 40});
    theme.search_match_foreground = marked_text;
    theme.occurrence_background = marked({214, 222, 236}, {70, 80, 100});
    theme.occurrence_foreground = marked_text;
    theme.bracket_match_background = marked({190, 226, 180}, {70, 110, 60});
    theme.bracket_match_foreground = marked_text;
    // Comparing: red and green deep enough for light text on a dark well, pale enough for dark text on a light one.
    theme.diff_removed_background = marked({240, 190, 180}, {120, 40, 40});
    theme.diff_removed_foreground = marked_text;
    theme.diff_added_background = marked({190, 226, 180}, {40, 105, 50});
    theme.diff_added_foreground = marked_text;
    theme.diff_changed_background = marked({200, 215, 235}, {50, 62, 86});
    theme.diff_changed_foreground = marked_text;
    theme.diff_filler_background = marked(mix(style.well, {0, 0, 0}, 30), {24, 24, 22});
    theme.scrollbar_background = style.shadow;
    theme.scrollbar_foreground = style.face;
    theme.editor_background = style.well;
    theme.editor_foreground = style.well_text;
    theme.reading_background = style.well;
    theme.reading_foreground = style.well_text;
    theme.side_panel_background = style.well;
    theme.syntax_keyword = style.syntax.keyword;
    theme.syntax_type = style.syntax.type;
    theme.syntax_function = style.syntax.function;
    theme.syntax_string = style.syntax.string;
    theme.syntax_number = style.syntax.number;
    theme.syntax_comment = style.syntax.comment;
    theme.syntax_preprocessor = style.syntax.preprocessor;
    theme.syntax_operator = style.syntax.operator_;
    theme.heading_foreground = style.syntax.keyword;
    theme.code_foreground = style.syntax.type;
    theme.code_background = mix(style.well, {0, 0, 0}, light_well ? 20 : 80);
    theme.link_foreground = style.syntax.string;
    theme.input_background = style.well;
    theme.input_foreground = style.well_text;
    theme.disabled_foreground = mix(style.face, text, 110);
    theme.line_number_foreground = theme.disabled_foreground;
    theme.hotkey_foreground = text;
    theme.error_foreground = marked({180, 30, 30}, {220, 80, 60});
    theme.warning_foreground = marked({150, 90, 0}, {232, 184, 72});
    theme.info_foreground = marked({0, 100, 150}, {120, 196, 208});
    return theme;
}

GraphicalPainter::GraphicalPainter(Canvas& canvas, const GraphicalStyle& style, Font& cell_font, Font& interface_font,
                                   Font& symbols, std::optional<PixelRect> area)
    : m_canvas{canvas}, m_style{style}, m_cell_font{cell_font}, m_interface_font{interface_font},
      m_fonts{cell_font, interface_font, symbols}, m_cell{cell_font.glyph(U'M').advance, cell_font.line_height()},
      m_area{area.value_or(PixelRect{0, 0, canvas.width(), canvas.height()})}
{
    m_canvas.set_clip(m_area);
}

Size GraphicalPainter::cell_size(Font& cell_font)
{
    return {static_cast<double>(cell_font.glyph(U'M').advance), static_cast<double>(cell_font.line_height())};
}

PixelRect GraphicalPainter::pixels(const Rect& cells) const
{
    // Each edge rounded on its own, so rectangles that meet in cells meet in pixels, with no gap and no overlap.
    const auto left{static_cast<int>(std::lround(cells.x * m_cell.width))};
    const auto right{static_cast<int>(std::lround((cells.x + cells.width) * m_cell.width))};
    const auto top{static_cast<int>(std::lround(cells.y * m_cell.height))};
    const auto bottom{static_cast<int>(std::lround((cells.y + cells.height) * m_cell.height))};
    return {m_area.x + left, m_area.y + top, right - left, bottom - top};
}

void GraphicalPainter::fill_rect(const Rect& rect, Color color)
{
    m_canvas.fill(pixels(rect), color);
}

void GraphicalPainter::draw_text(Point position, std::string_view text, const TextStyle& style)
{
    struct Placed
    {
        double column;
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
        const auto area{pixels({cell.column, position.y, static_cast<double>(cell.width), 1.0})};
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
    if(m_style.flat)
    {
        edges(rect, m_style.shadow, m_style.shadow);
        return;
    }
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

double GraphicalPainter::label_columns(std::string_view text)
{
    return label_columns(interface_width(text), m_style.text_margin,
                         {static_cast<double>(m_cell.width), static_cast<double>(m_cell.height)});
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
    const auto area{pixels({position.x, position.y, static_cast<double>(shown_width(text)), 1.0})};
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

void GraphicalPainter::draw_label(const Rect& area, std::string_view text, const TextStyle& style, Align align)
{
    const auto box{pixels(area)};
    // The background behind the text and its margins, where it is set, as the cells a terminal writes take theirs.
    const auto width{std::min(box.width, interface_width(text) + 2 * m_style.text_margin)};
    const auto left{align == Align::center ? box.x + (box.width - width) / 2
                    : align == Align::end  ? box.x + box.width - width
                                           : box.x};
    if(!text.empty())
    {
        m_canvas.fill({left, box.y, width, box.height}, style.background);
    }
    const auto foreground{(style.attributes & text_dim) != 0 ? mix(style.foreground, style.background, 110)
                                                             : style.foreground};
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip(intersect(previous, box));
    const auto x{label(with_margin(box, align), text, foreground, align, (style.attributes & text_bold) != 0)};
    if((style.attributes & text_underline) != 0)
    {
        const auto top{box.y + (box.height - m_interface_font.line_height()) / 2};
        m_canvas.fill({x, top + m_interface_font.ascent() + 2, interface_width(text), 1}, foreground);
    }
    m_canvas.set_clip(previous);
}

void GraphicalPainter::draw_label_runs(Point position, const std::vector<LabelRun>& runs, Color background)
{
    int columns{};
    for(const auto& run : runs)
    {
        columns += text_width(run.text);
    }
    const auto area{pixels({position.x, position.y, static_cast<double>(columns), 1.0})};
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
    const auto area{pixels({position.x, position.y, static_cast<double>(shown_width(text)), 1.0})};
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
    bevel(shape, look.pressed, look.pressed && m_style.flat ? m_style.light : theme.button_background);
    const auto shift{look.pressed && !m_style.flat ? 1 : 0};
    const auto ink{look.enabled ? theme.button_foreground : theme.disabled_foreground};
    if(look.icon)
    {
        const auto size{shape.height * 3 / 5};
        oui::draw_icon(
            m_canvas, *look.icon,
            {shape.x + shift + (shape.width - size) / 2, shape.y + shift + (shape.height - size) / 2, size, size}, ink);
    }
    else
    {
        label({shape.x + shift, shape.y + shift, shape.width, shape.height}, look.text, ink, Align::center);
    }
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
    // The title in the cells TabView gives it, a cell in, as TextMetrics::label_width measures it.
    const auto title_columns{label_columns(look.title)};
    label(pixels({bounds.x + 1, bounds.y, title_columns, 1}), look.title,
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
    // The thumb is in whole cells from the whole cell the track starts in, as a terminal shows it: here it starts as
    // far from the track's own start, which may fall between cells.
    if(track.width == 1)
    {
        const auto thumb{pixels(
            {track.x, track.y + (look.thumb_start - whole(track.y)), 1.0, static_cast<double>(look.thumb_length)})};
        bevel({area.x, thumb.y, area.width, thumb.height}, false, m_style.face);
        return;
    }
    // Across, a row is twice as tall as a column is wide: the bar takes the middle half of it.
    const PixelRect bar{area.x, area.y + area.height / 4, area.width, area.height / 2};
    bevel(bar, true, m_style.shadow);
    const auto thumb{
        pixels({track.x + (look.thumb_start - whole(track.x)), track.y, static_cast<double>(look.thumb_length), 1.0})};
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
    bevel(area, look.pressed, look.pressed && m_style.flat ? m_style.light : theme.button_background);
    const auto shift{look.pressed && !m_style.flat ? 1 : 0};
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
    default:
    {
        // As tall as a letter, wider than its cell.
        const auto size{static_cast<int>(16 * u)};
        oui::draw_icon(m_canvas, icon, {static_cast<int>(cx) - size / 2, static_cast<int>(cy) - size / 2, size, size},
                       style.foreground);
        break;
    }
    }
}

void GraphicalPainter::draw_spin_arrows(const Rect& cells, const Theme& theme)
{
    // A raised button the field's height and a little wider than it is tall, split across: up in its upper half, down
    // in its lower one.
    const auto area{pixels(cells)};
    const auto width{std::min(area.width, m_cell.height * 3 / 4)};
    const PixelRect button{area.x + area.width - width, area.y, width, area.height};
    m_canvas.fill(area, theme.dialog_background);
    const auto half{button.height / 2};
    bevel({button.x, button.y, button.width, half}, false, m_style.face);
    bevel({button.x, button.y + half, button.width, button.height - half}, false, m_style.face);
    const auto cx{static_cast<float>(button.x) + static_cast<float>(button.width) / 2};
    const auto u{unit()};
    const auto up_y{static_cast<float>(button.y) + static_cast<float>(half) / 2};
    const auto down_y{static_cast<float>(button.y + half) + static_cast<float>(button.height - half) / 2};
    m_canvas.fill_polygon({{cx - 4 * u, up_y + 2 * u}, {cx + 4 * u, up_y + 2 * u}, {cx, up_y - 2 * u}}, m_style.text);
    m_canvas.fill_polygon({{cx - 4 * u, down_y - 2 * u}, {cx + 4 * u, down_y - 2 * u}, {cx, down_y + 2 * u}},
                          m_style.text);
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
    const auto start{area.x + m_cell.width / 2};
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
    const TraceScope span{"oui.draw_image"};
    const auto area{pixels(bounds)};
    const auto previous{m_canvas.clip()};
    m_canvas.set_clip(intersect(previous, area));
    if(image.alpha)
    {
        m_canvas.mix_over(area.x, area.y, image.pixels, image.width, image.height, image.stride);
    }
    else
    {
        m_canvas.copy(area.x, area.y, image.pixels, image.width, image.height, image.stride);
    }
    m_canvas.set_clip(previous);
}

std::optional<Rect> GraphicalPainter::pixel_bounds(const Rect& cells) const
{
    const auto area{pixels(cells)};
    return Rect{static_cast<double>(area.x - m_area.x), static_cast<double>(area.y - m_area.y),
                static_cast<double>(area.width), static_cast<double>(area.height)};
}

int GraphicalPainter::field_padding(Size cell)
{
    return whole(cell.width) / 2;
}

double GraphicalPainter::label_columns(int pixels, int margin, Size cell)
{
    return pixels == 0 ? 0.0 : snap_up((pixels + 2 * margin) / cell.width, 1.0 / cell.width);
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
    const auto area{pixels({position.x, position.y, static_cast<double>(marker.columns), 1.0})};
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
    const auto length{std::max(0, whole(bounds.width) - words) * m_cell.width};
    const PixelRect track{area.x, area.y + area.height / 5, length, area.height - 2 * (area.height / 5)};
    bevel(track, true, m_style.well);
    const auto inside{PixelRect{track.x + m_style.line, track.y + m_style.line, track.width - 2 * m_style.line,
                                track.height - 2 * m_style.line}};
    const auto done{static_cast<int>(std::lround(std::clamp(look.done, 0.0, 1.0) * inside.width))};
    m_canvas.fill({inside.x, inside.y, done, inside.height}, m_style.title);
    if(words > 0)
    {
        static_cast<void>(
            label({area.x + length + m_cell.width, area.y, area.width - length - m_cell.width, area.height}, look.text,
                  m_style.text, Align::start));
    }
}

void GraphicalPainter::draw_slider(const Rect& bounds, const SliderLook& look, const Theme&)
{
    // A thin sunken track along the middle of the row and a raised thumb on it, as wide as the row is tall, its middle
    // where the value is; Slider follows the pointer by the same measures.
    const auto area{pixels(bounds)};
    if(area.width <= 1 || area.height <= 0)
    {
        return;
    }
    const auto thumb_width{std::min(area.height, area.width / 2)};
    const auto travel{std::max(1, area.width - thumb_width)};
    const auto track_height{std::max(4, area.height / 6)};
    bevel({area.x + thumb_width / 2, area.y + (area.height - track_height) / 2, area.width - thumb_width, track_height},
          true, m_style.well);
    const auto left{area.x + static_cast<int>(std::lround(std::clamp(look.position, 0.0, 1.0) * travel))};
    const PixelRect thumb{left, area.y + area.height / 8, thumb_width, area.height - 2 * (area.height / 8)};
    bevel(thumb, false, look.enabled ? m_style.face : m_style.shadow);
    if(look.focused)
    {
        focus_ring(thumb);
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
