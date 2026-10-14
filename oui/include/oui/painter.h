#pragma once

#include <oui/geometry.h>
#include <oui/color.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

inline constexpr std::uint8_t text_bold{1};
inline constexpr std::uint8_t text_underline{2};
inline constexpr std::uint8_t text_italic{4};
inline constexpr std::uint8_t text_dim{8};

struct TextStyle
{
    Color foreground{255, 255, 255};
    Color background{0, 0, 0};
    std::uint8_t attributes{0};
};

struct Theme;

enum class Align
{
    start,
    center,
    end
};

// What a widget asks the painter to draw: the painter decides how it looks. Each says what the element shows and
// in which state, never how; a terminal draws it with characters, a graphical style with its own shapes.
struct WindowLook
{
    std::string_view title;
    bool double_border{true};
    bool resizable{false};
    // Moved or resized from the keyboard right now.
    bool placing{false};
    // Shows a button that closes it, in the last two cells of its title row, where the style draws one.
    bool closable{false};
    // The close button is held down.
    bool close_pressed{false};
    // Buttons that leave it only in the task bar and that make it fill the screen, each in the two cells before the
    // last one's, where the style draws them; maximized, the second shows it puts the window back.
    bool minimizable{false};
    bool maximizable{false};
    bool maximized{false};
    Color background;
};

struct ButtonLook
{
    std::string_view text;
    bool pressed{false};
    bool focused{false};
    // A terminal button casts a shadow on the row below and the column to its right.
    bool shadow{true};
    // One row, its text a single mark such as ×.
    bool compact{false};
    // The button Enter presses from anywhere in its dialog.
    bool is_default{false};
    // Where in `text` the letter that presses it with Alt is, in bytes.
    std::optional<std::size_t> mnemonic;
    // What lies around the button, where its shadow falls.
    Color around;
    // A disabled button shows so, quieter.
    bool enabled{true};
};

struct CheckBoxLook
{
    std::string_view text;
    bool checked{false};
    bool focused{false};
    Color background;
    // Where in `text` the letter that turns it with Alt is, in bytes.
    std::optional<std::size_t> mnemonic{};
    // A disabled box shows so, quieter.
    bool enabled{true};
};

struct TabLook
{
    std::string_view title;
    bool selected{false};
    bool closable{false};
    // Asks for attention, such as a document with unsaved changes.
    bool highlighted{false};
};

struct ScrollbarLook
{
    // Where the thumb starts, as a row of the screen for a vertical track and a column for a horizontal one, not
    // counted from the track's start, and how many cells it takes.
    int thumb_start{0};
    int thumb_length{1};
    // How a terminal paints the track and the thumb.
    Color track;
    Color thumb;
};

// A field of editable text: what it shows, what is selected and where the cursor is, in bytes of the text.
struct TextFieldLook
{
    std::string_view text;
    // [selection_start, selection_end) is selected.
    std::size_t selection_start{0};
    std::size_t selection_end{0};
    // Only while it has the focus.
    std::optional<std::size_t> cursor;
    TextStyle style;
    TextStyle selected;
};

enum class BarKind
{
    menu,
    status
};

// Small marks that a terminal writes as a character and a graphical style draws as a shape.
enum class Icon
{
    submenu,
    drop_down,
    // The column a table is sorted by, up or down.
    sorted_up,
    sorted_down,
    // What an entry of a file list is.
    folder,
    file,
    parent_folder,
    // A task in a list of things to do, open or done.
    task_open,
    task_done
};

// A column's header in a table: its name, the arrow when the table is sorted by it, and whether it is held down.
struct HeaderLook
{
    std::string_view text;
    std::optional<Icon> sort;
    bool pressed{false};
};

// A mark before a text in a column of its own, as a folder's: a terminal writes `text`, a graphical style may draw
// `icon` instead. Texts with marks of the same width line up whether they show one or not.
struct Marker
{
    std::string_view text;
    int columns{0};
    std::optional<Icon> icon;
};

// A stretch of interface text in a style of its own, as a word in bold within a line.
struct LabelRun
{
    std::string_view text;
    TextStyle style;
};

// Pixels drawn somewhere else, such as another program's window: `width` by `height`, as 0x00RRGGBB, each row
// `stride` pixels after the one before. With `alpha`, they are 0xAARRGGBB with straight alpha, as an icon, and are
// mixed over what is under them.
struct PixelImage
{
    const std::uint32_t* pixels{};
    int width{};
    int height{};
    int stride{};
    bool alpha{false};
};

// How far a progress bar has gone, from 0 to 1, and what it says after it.
struct ProgressLook
{
    double done{0.0};
    std::string_view text;
};

// Where a slider's thumb is along its track, from 0 at the start to 1 at the end, and how it shows.
struct SliderLook
{
    double position{0.0};
    bool focused{false};
    bool enabled{true};
};

class Painter
{
  public:
    virtual ~Painter() = default;

    // Cells: text on the grid, one character per cell, as code and tables need.
    virtual void fill_rect(const Rect& rect, Color color) = 0;
    virtual void draw_text(Point position, std::string_view text, const TextStyle& style) = 0;
    virtual void push_clip(const Rect& rect) = 0;
    virtual void pop_clip() = 0;

    // Elements. Each draws within the cells it is given; what a terminal shows is the default, built on the cells
    // above, so a painter overrides only what it draws its own way.

    // Interface text, such as a label or a title, over the cells its characters take: a graphical style may set it in
    // a proportional font, placed in those cells as `align` says.
    virtual void draw_label(Point position, std::string_view text, const TextStyle& style, Align align = Align::start);
    // Runs of interface text one after another on a line, as prose with words in bold, code or links: a graphical
    // style sets them flowing in a proportional font over `background`, the line's own.
    virtual void draw_label_runs(Point position, const std::vector<LabelRun>& runs, Color background);
    // Interface text with the letter that opens it, at byte `mnemonic` of the text, underlined in `mnemonic_color`.
    virtual void draw_mnemonic_label(Point position, std::string_view text, std::size_t mnemonic,
                                     const TextStyle& style, Color mnemonic_color, Align align = Align::start);
    // A window's background, frame, title and shadow; its content is painted over it.
    virtual void draw_window(const Rect& bounds, const WindowLook& look, const Theme& theme);
    virtual void draw_button(const Rect& bounds, const ButtonLook& look, const Theme& theme);
    virtual void draw_check_box(const Rect& bounds, const CheckBoxLook& look, const Theme& theme);
    // One tab of a tab bar, in the cells its title and close mark take.
    virtual void draw_tab(const Rect& bounds, const TabLook& look, const Theme& theme);
    // A column's header, one row high; a terminal writes it on the header row's colours.
    virtual void draw_header(const Rect& cell, const HeaderLook& look, const Theme& theme);
    // The line between the parts of a split view: a column or a row.
    virtual void draw_divider(const Rect& line, const Theme& theme);
    // A scrollbar along `track`, a column or a row of cells.
    virtual void draw_scrollbar(const Rect& track, const ScrollbarLook& look, const Theme& theme);
    // The background of a menu bar or a status bar; its text is drawn over it.
    virtual void draw_bar(const Rect& bounds, BarKind kind, const Theme& theme);
    // A drop-down menu's background, frame and shadow; its entries are drawn over it.
    virtual void draw_popup(const Rect& bounds, const Theme& theme);
    // The edge of a drop-down menu, drawn after its entries: the terminal's frame is all in draw_popup.
    virtual void draw_popup_edge(const Rect& bounds, const Theme& theme);
    // The line between groups of a drop-down menu's entries, across the row of the popup it is on.
    virtual void draw_popup_separator(const Rect& row, const Theme& theme);
    virtual void draw_icon(Point position, Icon icon, const TextStyle& style);
    // A number field's arrows that step its value, in `cells` at its right, two cells: side by side, down then up, as
    // a terminal can only draw them, or one over the other, up then down, as a window does.
    virtual void draw_spin_arrows(const Rect& cells, const Theme& theme);
    // Whether draw_spin_arrows puts them one over the other, so the upper half of the cells steps up.
    [[nodiscard]] virtual bool stacks_spin_arrows() const { return false; }
    virtual void draw_marker(Point position, const Marker& marker, const TextStyle& style);
    // The mark before a checkable entry, as in a menu: four cells, "[■] " or "[ ] " in a terminal.
    virtual void draw_check_mark(Point position, bool checked, const TextStyle& style);
    // An editor's cursor at a cell, over the `character` already drawn there in `style`: in a terminal the character
    // reversed, a block; in a window a thin bar at the cell's left, as a text field's.
    virtual void draw_text_cursor(Point position, std::string_view character, const TextStyle& style);
    // The text of a text field, with its selection and its cursor, over the whole field.
    virtual void draw_text_field(const Rect& bounds, const TextFieldLook& look, const Theme& theme);
    // A tooltip over everything else, its lines in `bounds`: in a terminal, the cells on the menu's colours; in pixels,
    // framed, and see-through enough that what it covers still shows.
    virtual void draw_tooltip(const Rect& bounds, const std::vector<std::string>& lines, const Theme& theme);
    // What a drag carries, at the pointer, over everything else.
    virtual void draw_drag_image(Point position, int cell_offset, std::string_view text, const Theme& theme);
    // A progress bar over the cells of `bounds`: its track, as much of it filled as is done, and its text after it.
    virtual void draw_progress(const Rect& bounds, const ProgressLook& look, const Theme& theme);
    // A slider in one row: the track along it and the thumb where the value is.
    virtual void draw_slider(const Rect& bounds, const SliderLook& look, const Theme& theme);
    // The edge of a text field, drawn after its content: the terminal shows none.
    virtual void draw_field_frame(const Rect& bounds, bool focused, const Theme& theme);
    // Pixels from the top left corner of `bounds`, cut to it: a terminal, which has none, shows the background.
    virtual void draw_image(const Rect& bounds, const PixelImage& image, const Theme& theme);
    // Where `cells` are in pixels, from the top left of the application's area, as MouseEvent::pixel counts, where the
    // painter draws in pixels; none in a terminal. Rows are not all the same height, so this is the only sure way.
    [[nodiscard]] virtual std::optional<Rect> pixel_bounds(const Rect& cells) const;
    // Casts a shadow over what is already painted in `rect`: in cells, what is there stays visible, in `foreground` on
    // `background`, as Turbo Vision's shadows; elsewhere the area takes `background`.
    virtual void shade(const Rect& rect, Color foreground, Color background);
};

} // namespace olagarro::oui
