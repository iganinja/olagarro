#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/theme.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cmath>
#include <string>

// The elements as a terminal shows them, built from cells.

namespace olagarro::oui
{

namespace
{

std::string repeated(std::string_view text, int count)
{
    std::string result;
    for(int index{}; index < count; ++ index)
    {
        result += text;
    }
    return result;
}

void clear_rect(Painter& painter, const Rect& rect, Color background)
{
    // Only the background shows: the cells hold spaces.
    if(rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
    const std::string spaces(static_cast<std::size_t>(rect.width), ' ');
    const TextStyle style{background, background};
    for(auto y{rect.y}; y < rect.y + rect.height; ++ y)
    {
        painter.draw_text({rect.x, y}, spaces, style);
    }
}

} // namespace

void Painter::draw_label(Point position, std::string_view text, const TextStyle& style, Align)
{
    draw_text(position, text, style);
}

void Painter::draw_label_runs(Point position, const std::vector<LabelRun>& runs, Color)
{
    for(const auto& run : runs)
    {
        draw_label(position, run.text, run.style);
        position.x += text_width(run.text);
    }
}

void Painter::draw_mnemonic_label(Point position, std::string_view text, std::size_t mnemonic, const TextStyle& style,
                                  Color mnemonic_color, Align)
{
    draw_label(position, text, style);
    auto end{mnemonic};
    static_cast<void>(decode_utf8(text, end));
    auto underlined{style};
    underlined.attributes |= text_underline;
    underlined.foreground = mnemonic_color;
    draw_label({position.x + text_width(text.substr(0, mnemonic)), position.y}, text.substr(mnemonic, end - mnemonic),
               underlined);
}

void Painter::draw_window(const Rect& bounds, const WindowLook& look, const Theme& theme)
{
    const auto bg{look.background};
    // Its shadow, as Turbo Vision's: two columns at its right and a row under it, a row and two columns in, over
    // whatever is there.
    shade({bounds.x + bounds.width, bounds.y + 1, 2, bounds.height}, theme.shadow_foreground, theme.shadow);
    shade({bounds.x + 2, bounds.y + bounds.height, bounds.width, 1}, theme.shadow_foreground, theme.shadow);
    clear_rect(*this, bounds, bg);
    // Placing from the keyboard draws the frame single, as Turbo Vision does while a window is dragged.
    const auto double_border{look.double_border && !look.placing};
    const std::string_view horizontal{double_border ? "═" : "─"};
    const std::string_view vertical{double_border ? "║" : "│"};
    const std::string_view top_left{double_border ? "╔" : "┌"};
    const std::string_view top_right{double_border ? "╗" : "┐"};
    const std::string_view bottom_left{double_border ? "╚" : "└"};
    const std::string_view bottom_right{look.resizable ? "┘" : double_border ? "╝" : "┘"};
    // While placed from the keyboard the frame stands out, so it is clear which window the arrows move.
    const auto border{look.placing ? theme.dialog_border_placing : theme.dialog_border};
    const auto right{bounds.x + bounds.width - 1};
    const auto bottom{bounds.y + bounds.height - 1};
    draw_text({bounds.x, bounds.y}, top_left, {border, bg});
    draw_text({right, bounds.y}, top_right, {border, bg});
    draw_text({bounds.x, bottom}, bottom_left, {border, bg});
    draw_text({right, bottom}, bottom_right, {border, bg});
    for(auto x{bounds.x + 1}; x < right; ++ x)
    {
        draw_text({x, bounds.y}, horizontal, {border, bg});
        draw_text({x, bottom}, horizontal, {border, bg});
    }
    for(auto y{bounds.y + 1}; y < bottom; ++ y)
    {
        draw_text({bounds.x, y}, vertical, {border, bg});
        draw_text({right, y}, vertical, {border, bg});
    }
    if(look.resizable && bounds.width > 2)
    {
        // Turbo Vision's resize grip: a single-line corner, visible even on a double frame.
        draw_text({right - 1, bottom}, "─", {border, bg});
    }
    // A mark on the top edge, between brackets of the frame's colour.
    const auto mark{[&](int x, std::string_view icon)
                    {
                        draw_text({x, bounds.y}, "[", {border, bg});
                        draw_text({x + 1, bounds.y}, icon, {theme.dialog_icon_foreground, bg});
                        draw_text({x + 2, bounds.y}, "]", {border, bg});
                    }};
    // A dialog's close mark goes at the left, as Turbo Vision's; a window of the desktop keeps its three before the
    // right corner, close last, then maximize, then minimize.
    const auto on_the_right{look.minimizable || look.maximizable};
    auto title_start{bounds.x + 1};
    auto title_end{right};
    if(look.closable && !on_the_right && bounds.width > 8)
    {
        mark(bounds.x + 2, "■");
        title_start = bounds.x + 5;
    }
    if(on_the_right)
    {
        auto button_x{right - 4};
        const auto button{[&](std::string_view icon)
                          {
                              if(button_x > bounds.x)
                              {
                                  mark(button_x, icon);
                              }
                              button_x -= 3;
                          }};
        if(look.closable)
        {
            button("■");
        }
        if(look.maximizable)
        {
            button(look.maximized ? "↕" : "↑");
        }
        if(look.minimizable)
        {
            button("↓");
        }
        title_end = button_x + 3;
    }
    // The title in the middle of the room left, a space at each side.
    if(const auto room{title_end - title_start - 2}; room > 2 && !look.title.empty())
    {
        const auto title{" " + fit_text(look.title, room - 2) + " "};
        const auto width{text_width(title)};
        const auto centred{bounds.x + (bounds.width - width) / 2};
        draw_text({std::clamp(centred, title_start, std::max(title_start, title_end - width)), bounds.y}, title,
                  {border, bg});
    }
}

void Painter::draw_button(const Rect& bounds, const ButtonLook& look, const Theme& theme)
{
    const auto bg{look.pressed ? theme.button_pressed_background
                               : (look.focused ? theme.button_focused_background : theme.button_background)};
    const auto fg{!look.enabled     ? theme.disabled_foreground
                  : look.pressed    ? theme.button_pressed_foreground
                  : look.focused    ? theme.button_focused_foreground
                  : look.is_default ? theme.button_default_foreground
                                    : theme.button_foreground};
    const auto x{bounds.x + (look.pressed ? 1 : 0)};
    const auto y{bounds.y};
    const auto w{std::max(1, bounds.width - (look.shadow ? 1 : 0))};
    fill_rect({x, y, w, 1}, bg);
    const auto text_x{x + std::max(0, (w - text_width(look.text)) / 2)};
    draw_text({text_x, y}, look.text, {fg, bg});
    if(look.enabled && look.mnemonic && *look.mnemonic < look.text.size())
    {
        auto end{*look.mnemonic};
        static_cast<void>(decode_utf8(look.text, end));
        draw_text({text_x + text_width(look.text.substr(0, *look.mnemonic)), y},
                  look.text.substr(*look.mnemonic, end - *look.mnemonic), {theme.button_shortcut_foreground, bg});
    }
    if(!look.pressed && look.shadow)
    {
        draw_text({bounds.x + 1, y + 1}, repeated("▀", w + 1), {theme.shadow, look.around});
        draw_text({x + w, y}, "▄", {theme.shadow, look.around});
    }
}

void Painter::draw_check_box(const Rect& bounds, const CheckBoxLook& look, const Theme& theme)
{
    // As a Turbo Vision cluster: its own colour across the row, the one with the focus in another.
    fill_rect(bounds, theme.cluster_background);
    const TextStyle style{!look.enabled  ? theme.disabled_foreground
                          : look.focused ? theme.cluster_focused_foreground
                                         : theme.cluster_foreground,
                          theme.cluster_background};
    draw_text({bounds.x, bounds.y}, look.checked ? "[X] " : "[ ] ", style);
    draw_label({bounds.x + 4, bounds.y}, look.text, style);
    if(look.enabled && look.mnemonic && *look.mnemonic < look.text.size())
    {
        auto end{*look.mnemonic};
        static_cast<void>(decode_utf8(look.text, end));
        draw_text({bounds.x + 4 + text_width(look.text.substr(0, *look.mnemonic)), bounds.y},
                  look.text.substr(*look.mnemonic, end - *look.mnemonic),
                  {theme.cluster_shortcut_foreground, theme.cluster_background});
    }
}

void Painter::draw_tab(const Rect& bounds, const TabLook& look, const Theme& theme)
{
    const auto background{look.selected ? theme.tab_active_background : theme.tab_inactive_background};
    TextStyle title_style{look.selected ? theme.tab_active_foreground : theme.tab_inactive_foreground, background};
    if(look.selected)
    {
        title_style.attributes |= text_bold;
    }
    draw_label({bounds.x, bounds.y}, " " + std::string{look.title} + " ", title_style);
    if(look.closable)
    {
        const auto close_fg{look.highlighted ? theme.warning_foreground
                            : look.selected  ? theme.tab_active_foreground
                                             : theme.tab_inactive_foreground};
        draw_text({bounds.x + text_width(look.title) + 2, bounds.y}, "× ", {close_fg, background});
    }
}

void Painter::draw_header(const Rect& cell, const HeaderLook& look, const Theme& theme)
{
    // Held down, the header shows in reverse; the column sorted by is in bold, its arrow after the name.
    TextStyle style{look.pressed ? theme.table_header_background : theme.table_header_foreground,
                    look.pressed ? theme.table_header_foreground : theme.table_header_background};
    fill_rect(cell, style.background);
    const std::string text{look.text};
    if(!look.sort)
    {
        draw_label({cell.x, cell.y}, fit_text(text, cell.width), style);
        return;
    }
    style.attributes |= text_bold;
    const auto fitted{fit_text(text + " ▲", cell.width)};
    if(fitted != text + " ▲")
    {
        draw_label({cell.x, cell.y}, fitted, style);
        return;
    }
    // The arrow is a mark of its own, so a graphical style draws it as a shape.
    draw_label({cell.x, cell.y}, text + " ", style);
    draw_icon({cell.x + text_width(text) + 1, cell.y}, *look.sort, style);
}

void Painter::draw_divider(const Rect& line, const Theme& theme)
{
    const TextStyle style{theme.panel_foreground, theme.panel_background};
    if(line.width == 1)
    {
        for(auto y{line.y}; y < line.y + line.height; ++ y)
        {
            draw_text({line.x, y}, "│", style);
        }
        return;
    }
    draw_text({line.x, line.y}, repeated("─", line.width), style);
}

void Painter::draw_scrollbar(const Rect& track, const ScrollbarLook& look, const Theme&)
{
    // As Turbo Vision's, without its arrows: a shaded track and on it a one-cell indicator of where the view is,
    // however much of it shows.
    const auto vertical{track.width == 1};
    const auto length{vertical ? track.height : track.width};
    const TextStyle style{look.thumb, look.track};
    const auto at{[&](int offset)
                  { return vertical ? Point{track.x, track.y + offset} : Point{track.x + offset, track.y}; }};
    for(int offset{}; offset < length; ++ offset)
    {
        draw_text(at(offset), "▒", style);
    }
    if(length < 1)
    {
        return;
    }
    // Where the thumb is along what it can travel, as a place on the track.
    const auto start{look.thumb_start - (vertical ? track.y : track.x)};
    const auto travel{length - look.thumb_length};
    const auto place{travel <= 0 ? 0 : (start * (length - 1) + travel / 2) / travel};
    draw_text(at(std::clamp(place, 0, length - 1)), "■", style);
}

void Painter::draw_text_cursor(Point position, std::string_view character, const TextStyle& style)
{
    draw_text(position, character, {style.background, style.foreground, style.attributes});
}

void Painter::draw_progress(const Rect& bounds, const ProgressLook& look, const Theme& theme)
{
    // The track in whole cells, what is done of it filled, and the cell it ends in filled by eighths, as far as it
    // goes.
    static constexpr std::string_view eighths[]{"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    const auto words{look.text.empty() ? 0 : shown_width(look.text) + 1};
    const auto length{std::max(0, bounds.width - words)};
    const auto done{static_cast<int>(std::lround(std::clamp(look.done, 0.0, 1.0) * length * 8))};
    for(int cell{}; cell < length; ++ cell)
    {
        const Point at{bounds.x + cell, bounds.y};
        const auto filled{done - cell * 8};
        if(filled >= 8)
        {
            draw_text(at, " ", {theme.item_selected_foreground, theme.item_selected_background});
        }
        else if(filled > 0)
        {
            draw_text(at, eighths[filled], {theme.item_selected_background, theme.scrollbar_background});
        }
        else
        {
            draw_text(at, " ", {theme.scrollbar_foreground, theme.scrollbar_background});
        }
    }
    if(words > 0)
    {
        const TextStyle style{theme.dialog_foreground, theme.dialog_background};
        draw_text({bounds.x + length, bounds.y}, " ", style);
        draw_label({bounds.x + length + 1, bounds.y}, look.text, style);
    }
}

void Painter::draw_bar(const Rect& bounds, BarKind kind, const Theme& theme)
{
    clear_rect(*this, bounds, kind == BarKind::menu ? theme.menu_background : theme.status_background);
}

void Painter::draw_popup(const Rect& bounds, const Theme& theme)
{
    shade({bounds.x + bounds.width, bounds.y + 1, 2, bounds.height}, theme.shadow_foreground, theme.shadow);
    shade({bounds.x + 2, bounds.y + bounds.height, bounds.width, 1}, theme.shadow_foreground, theme.shadow);
    fill_rect(bounds, theme.menu_background);
    const TextStyle style{theme.menu_border, theme.menu_background};
    const auto horizontal{repeated("─", bounds.width - 2)};
    draw_text({bounds.x, bounds.y}, "┌" + horizontal + "┐", style);
    draw_text({bounds.x, bounds.y + bounds.height - 1}, "└" + horizontal + "┘", style);
    for(auto y{bounds.y + 1}; y < bounds.y + bounds.height - 1; ++ y)
    {
        draw_text({bounds.x, y}, "│", style);
        draw_text({bounds.x + bounds.width - 1, y}, "│", style);
    }
}

void Painter::draw_popup_edge(const Rect&, const Theme&)
{
}

void Painter::draw_popup_separator(const Rect& row, const Theme& theme)
{
    draw_text({row.x, row.y}, "├" + repeated("─", row.width - 2) + "┤", {theme.menu_border, theme.menu_background});
}

void Painter::draw_icon(Point position, Icon icon, const TextStyle& style)
{
    switch(icon)
    {
    case Icon::submenu:
        draw_text(position, "▶", style);
        break;
    case Icon::drop_down:
        draw_text(position, "▾", style);
        break;
    case Icon::sorted_up:
        draw_text(position, "▲", style);
        break;
    case Icon::sorted_down:
        draw_text(position, "▼", style);
        break;
    case Icon::folder:
        draw_text(position, "+", style);
        break;
    case Icon::file:
        draw_text(position, " ", style);
        break;
    case Icon::parent_folder:
        draw_text(position, "..", style);
        break;
    case Icon::task_open:
        draw_text(position, "[ ]", style);
        break;
    case Icon::task_done:
        draw_text(position, "[x]", style);
        break;
    }
}

void Painter::draw_marker(Point position, const Marker& marker, const TextStyle& style)
{
    const auto padding{std::max(0, marker.columns - text_width(marker.text))};
    draw_text(position, std::string{marker.text} + std::string(static_cast<std::size_t>(padding), ' '), style);
}

void Painter::draw_check_mark(Point position, bool checked, const TextStyle& style)
{
    draw_text(position, checked ? "[■] " : "[ ] ", style);
}

void Painter::draw_text_field(const Rect& bounds, const TextFieldLook& look, const Theme&)
{
    fill_rect(bounds, look.style.background);
    auto x{bounds.x};
    for(std::size_t index{}; index < look.text.size();)
    {
        const auto start{index};
        const char32_t glyph{decode_utf8(look.text, index)};
        const auto selected{start >= look.selection_start && start < look.selection_end};
        draw_text({x, bounds.y}, encode_utf8(glyph), selected ? look.selected : look.style);
        x += terminal_cell_width(glyph);
    }
    if(!look.cursor)
    {
        return;
    }
    const auto cursor_x{bounds.x + text_width(look.text.substr(0, *look.cursor))};
    if(cursor_x >= bounds.x + bounds.width)
    {
        return;
    }
    // A block cursor: the character under it in reverse.
    fill_rect({cursor_x, bounds.y, 1, 1}, look.style.foreground);
    if(*look.cursor < look.text.size())
    {
        std::size_t index{*look.cursor};
        draw_text({cursor_x, bounds.y}, encode_utf8(decode_utf8(look.text, index)),
                  {look.style.background, look.style.foreground});
    }
}

void Painter::draw_tooltip(const Rect& bounds, const std::vector<std::string>& lines, const Theme& theme)
{
    for(int row{}; row < bounds.height && row < static_cast<int>(lines.size()); ++ row)
    {
        const auto& line{lines[static_cast<std::size_t>(row)]};
        draw_label({bounds.x, bounds.y + row},
                   " " + line +
                       std::string(static_cast<std::size_t>(std::max(0, bounds.width - 1 - text_width(line))), ' '),
                   {theme.menu_foreground, theme.menu_background});
    }
}

void Painter::draw_drag_image(Point position, int, std::string_view text, const Theme& theme)
{
    draw_label(position, " " + std::string{text} + " ", {theme.selection_foreground, theme.selection_background});
}

void Painter::draw_field_frame(const Rect&, bool, const Theme&)
{
}

void Painter::draw_image(const Rect& bounds, const PixelImage&, const Theme& theme)
{
    fill_rect(bounds, theme.background);
}

std::optional<Rect> Painter::pixel_bounds(const Rect&) const
{
    return std::nullopt;
}

void Painter::shade(const Rect& rect, Color, Color background)
{
    fill_rect(rect, background);
}

} // namespace olagarro::oui
