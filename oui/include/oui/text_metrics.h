#pragma once

#include <oui/painter.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

// How wide interface text is where the application shows: in a terminal a character takes its cells, so this
// measures columns; a graphical backend measures pixels of its proportional font. Widgets that place a cursor
// in interface text, as a text field does, measure with it. Positions are in the same units, from the left of
// the text.
class TextMetrics
{
  public:
    virtual ~TextMetrics() = default;

    // How many units a cell is wide.
    [[nodiscard]] virtual int cell_width() const;
    // How fine a position may be, in cells across and down: a whole cell in a terminal, a pixel in a window.
    // Containers snap every edge they compute to it.
    [[nodiscard]] virtual double column_step() const;
    [[nodiscard]] virtual double row_step() const;
    // The size of a cell in pixels where the backend draws pixels; none in a terminal.
    [[nodiscard]] virtual std::optional<Size> cell_pixels() const;
    [[nodiscard]] virtual int width(std::string_view text) const;
    // How many cells interface text takes as a label shows it, with the margins it keeps from its edges: its columns
    // in a terminal; in a window its pixels and margins, up to the column step. No text takes none.
    [[nodiscard]] virtual double label_width(std::string_view text) const;
    // How far from the left edge of a text field its text starts.
    [[nodiscard]] virtual int field_padding() const;
    // How many rows the frame of a drop-down menu takes above and below its entries: a terminal draws a line in
    // each; a graphical style draws a thin edge within the entries' own rows.
    [[nodiscard]] virtual int popup_frame_rows() const;
    // Columns between panels side by side, as a file dialog's places and files: a terminal's panels meet, each in
    // its colour; a graphical style parts them.
    [[nodiscard]] virtual int panel_gap() const;
    // Rows a framed window's top takes before what it holds: a terminal's frame line, the title in it; a graphical
    // style's title bar and a row under it, as much room as is left under what it holds.
    [[nodiscard]] virtual int window_top_rows() const;
    // Columns a framed window keeps either side of what it holds: a terminal's frame line and a cell beside it; a
    // graphical style's thin edge needs only the one.
    [[nodiscard]] virtual int window_side_columns() const;
    // Whether a window's title bar has a button that closes it: a graphical style draws one.
    [[nodiscard]] virtual bool window_close_button() const;
    // The byte of `text` at `position`: where the character there starts, or with `through` where it ends, so a
    // drag takes in the character under the pointer. A proportional font takes the nearer edge of the character
    // instead, whatever `through` says.
    [[nodiscard]] virtual std::size_t byte_at(std::string_view text, int position, bool through) const;
};

// What a drag in progress shows under the pointer, as a tab dragged by its title: drawn over everything.
struct DragImage
{
    std::string text;
    Point position;
    // Where in its cell the pointer is, as MouseEvent::cell_offset.
    int cell_offset{0};
};

// Shows a drag image, or none; the widget dragging sets it as the pointer moves and clears it when let go.
void set_drag_image(std::optional<DragImage> image);
[[nodiscard]] const std::optional<DragImage>& drag_image();

// The metrics of whatever shows the application: a terminal's unless a backend set its own.
[[nodiscard]] const TextMetrics& text_metrics();
// Sets them, or puts back the terminal's with nullptr; a backend sets its own while it lives.
void set_text_metrics(const TextMetrics* metrics);

// `value` down to a whole number of `step`s; the tolerance takes a 2.9999999 that means 3 as 3. With a step of 1 it is
// what an int division gave, which keeps a terminal as it was.
[[nodiscard]] double snap(double value, double step);
// `value` up to a whole number of `step`s.
[[nodiscard]] double snap_up(double value, double step);
// `value` down to text_metrics()'s step across, and down.
[[nodiscard]] double snap_x(double value);
[[nodiscard]] double snap_y(double value);

} // namespace olagarro::oui
