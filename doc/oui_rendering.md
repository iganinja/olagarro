# Rendering in OUI

How OUI draws: the painter widgets draw on, in cells and in "elements" each style draws its own way; the backends
that turn a frame into characters in a terminal or pixels in a window, on a whole screen or in memory; the cell buffer
and how a terminal is updated; how theme colours reach a terminal; the fonts; text measured in a proportional font;
zoom; and how a widget writes its `paint()`.

- [The frame](#the-frame)
- [The painter](#the-painter)
- [Elements](#elements)
- [Backends](#backends)
- [The cell buffer](#the-cell-buffer)
- [Writing to a terminal](#writing-to-a-terminal)
- [Colours in a terminal](#colours-in-a-terminal)
- [Pixels: the graphical painter](#pixels-the-graphical-painter)
- [Fonts](#fonts)
- [Text metrics and proportional text](#text-metrics-and-proportional-text)
- [Zoom](#zoom)
- [Writing a widget's paint()](#writing-a-widgets-paint)
- [Rendering in tests](#rendering-in-tests)

The colours themselves come from the theme, described in [themes](themes.md); layout, which decides each widget's
cells, in [layout](oui_layout.md).

## The frame

Everything in OUI is laid out and painted in **cells**: a grid of columns and rows, as a terminal has. A backend says
how many cells there are (`Backend::size()`); widgets are arranged in them and paint in them, whatever the backend
shows them with. That is why one program runs unchanged in a terminal and in a window: only the backend and the
painter change, and layout stays in cells.

`Application::run_frame` ([application.cpp](../oui/src/application.cpp)) runs timers and posted work, hands the
backend's events to the widgets, and paints only when something asked for it (`m_needs_render`); with nothing to do
it sleeps in `Backend::wait_for_input` until the next event or timer, so an idle program uses no CPU.
`Application::render` does a frame:

```cpp
const auto size{m_backend->size()};
m_content->arrange({0, 0, size.width, size.height});
m_backend->render(
    [&](Painter& painter)
    {
        m_content->paint(painter, m_theme);
        // then the open popup, the tooltip, the hint of a keyboard mode and what a drag carries, over it all
    });
```

The whole frame is painted each time, from the back to the front. A backend of cells keeps the previous frame and
sends only what changed; see [writing to a terminal](#writing-to-a-terminal).

### Tracing a frame

A program measuring itself sets a `TraceSink` with `set_trace_sink` ([trace.h](../oui/include/oui/trace.h)) and hears
OUI's stages as spans that nest on their thread, each a `begin(name)` and an `end()`: `oui.frame` (all but the
sleep), and in it `oui.timers`, `oui.posted`, `oui.events` and `oui.render`; in that, `oui.layout` and, from a pixel
backend, `oui.paint`, `oui.decorate` and `oui.present` (the window's own show, the SDL upload and present); in
`oui.paint`, each widget's paint, named after its id or, without one, its type (`Button`), nested as the widgets are,
and the graphical painter's `oui.draw_image`. A program's own spans go to its sink as it likes. With no sink, the
default, a `TraceScope` costs one atomic read.

## The painter

`Painter` ([painter.h](../oui/include/oui/painter.h)) is what `Widget::paint(Painter&, const Theme&)` draws on.
It has two levels.

**Cells**, which every painter implements:

| Call | Does |
|------|------|
| `fill_rect(rect, color)` | Blanks the cells of `rect` with `color` as their background |
| `draw_text(position, text, style)` | Writes UTF-8 text from `position`, one character per cell, two for a wide one |
| `push_clip(rect)`, `pop_clip()` | Narrows where anything is drawn, to the intersection with the current clip; a stack |
| `shade(rect, foreground, background)` | Casts a shadow: in cells the characters stay, recoloured, as Turbo Vision's shadows |

`TextStyle` is a foreground, a background and attributes: `text_bold`, `text_underline`, `text_italic`, `text_dim`,
which can be combined. Positions and rectangles (`Point`, `Rect`) are in cells, as `double`s; a painter of cells takes
the cell each falls in, with `whole`.

**Elements** are the parts of an interface a style draws its own way: a window, a button, a tab. A widget does not
draw a button out of characters; it asks for one, saying what it shows and in which state, and the painter decides
how it looks. [Elements](#elements) lists them.

Two painters exist:

- `CellBufferPainter` ([cell_buffer.h](../oui/include/oui/cell_buffer.h)) writes cells into a `CellBuffer`. It
  implements only the cell calls; the elements come from the `Painter` base class, whose defaults in
  [painter.cpp](../oui/src/painter.cpp) are the terminal look, built on the cell calls: double-line frames,
  `▒` scrollbar tracks, `[■]` check marks, Turbo Vision shadows.
- `GraphicalPainter` ([graphical_painter.h](../oui/include/oui/graphics/graphical_painter.h)) paints pixels on a
  `Canvas` and overrides every element with bevelled shapes, interface text in a proportional font and cells in a
  monospace one. See [pixels](#pixels-the-graphical-painter).

A new look for characters would be another painter overriding some elements; a new look for pixels is mostly a
different [GraphicalStyle](themes.md#graphical-themes).

## Elements

Each element is a virtual method of `Painter` taking the cells it may use, a small "look" struct, and usually the
theme. The look says what the element shows, never how.

| Element | Look | Used for |
|---------|------|----------|
| `draw_label` | a position or an area, text, `TextStyle`, `Align` | Interface text: a label, a title. A graphical style sets it in a proportional font within the cells, aligned as asked; given an area, within it and cut to it, as a widget measured by `label_width` draws its text in its bounds |
| `draw_label_runs` | `LabelRun`s, line background | Runs of interface text in their own styles on one line, as prose with bold words, code or links |
| `draw_mnemonic_label` | text, byte of the mnemonic, its colour | Interface text with the letter Alt opens underlined |
| `draw_window` | `WindowLook`: title, double or single frame, resizable, placing, closable, minimizable, maximizable, background | A window's background, frame, title, buttons and shadow; its content paints over it |
| `draw_button` | `ButtonLook`: text, pressed, focused, shadow, compact, default, mnemonic, the colour around it, enabled, icon | A push button; a graphical style draws its icon, when it has one, in place of the text (`Button::set_icon`) |
| `draw_check_box` | `CheckBoxLook`: text, checked, focused, background, mnemonic, enabled | A check box with its text |
| `draw_tab` | `TabLook`: title, selected, closable, highlighted | One tab of a tab bar |
| `draw_header` | `HeaderLook`: text, sort arrow, pressed | A table column's header |
| `draw_divider` | | The line between the parts of a split view |
| `draw_scrollbar` | `ScrollbarLook`: thumb start and length, track and thumb colours | A scrollbar along a column or a row |
| `draw_bar` | `BarKind::menu` or `status` | The background of a menu bar or status bar |
| `draw_popup`, `draw_popup_edge`, `draw_popup_separator` | | A drop-down menu's background and frame, its edge drawn after the entries, and a line between groups |
| `draw_icon` | `Icon` | Small marks: submenu and drop-down arrows, sort arrows, folder, file, parent folder, open and done tasks; a player's play, pause, step back and step forwards; a track's speaker, headphones, eye and lock. A program drawing its own pixels draws them with the free `draw_icon(canvas, icon, area, colour)` of [graphical_painter.h](../oui/include/oui/graphics/graphical_painter.h) |
| `draw_marker` | `Marker`: text, columns, optional icon | A mark before a text in a column of its own, as a folder's; a terminal writes the text, a style may draw the icon |
| `draw_check_mark` | checked | The four cells before a checkable menu entry, `[■] ` or `[ ] ` in a terminal |
| `draw_text_field` | `TextFieldLook`: text, selection, cursor, styles | The text of a field with its selection and cursor |
| `draw_field_frame` | focused | A text field's edge, drawn after its content; a terminal shows none |
| `draw_tooltip` | lines | A tooltip over everything; in pixels framed and partly see-through |
| `draw_drag_image` | text, cell offset | What a drag carries, at the pointer |
| `draw_image` | `PixelImage` | Pixels from elsewhere, as another program's window on OUI's desktop; a terminal shows the background |

Some elements answer differences in shape that layout must know about, as how many rows a window's top takes: those
are in [text metrics](#text-metrics-and-proportional-text), not in the painter, since layout runs before painting.

## Backends

A `Backend` ([backend.h](../oui/include/oui/backend.h)) is what shows the application and brings its input:

```cpp
class Backend
{
  public:
    virtual Size size() const = 0;                       // in cells
    virtual std::optional<Event> poll_event() = 0;
    virtual void present(const CellBuffer& buffer) = 0;
    virtual void render(const std::function<void(Painter&)>& paint);
    virtual void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline);
    virtual void wake();
    // and: reset, reports_modifiers, the clipboard, set_title, zooms/zoom_by/zoom_reset, set_pointer
};
```

`render` is what the application calls. Its default ([backend.cpp](../oui/src/backend.cpp)) is the one for cells:
make a `CellBuffer` of `size()`, paint it with a `CellBufferPainter`, and `present` it. A graphical backend overrides
`render` to paint pixels with its own painter instead.

| Backend | Where | Shows |
|---------|-------|-------|
| `AnsiTerminalBackend`, from `terminal_backend()` | [terminal_backend.h](../oui/include/oui/terminal_backend.h); `terminal_backend_posix.cpp`, `terminal_backend_win32.cpp` | The terminal the program runs in, locally or over SSH, with escape sequences; on Windows also the legacy console API |
| `ScriptedBackend`, from `scripted_backend()` | [scripted_backend.h](../oui/include/oui/scripted_backend.h) | Nothing: replays a list of events and keeps the frames, for tests and automation |
| `RasterBackend` | [raster_backend.h](../oui/include/oui/raster_backend.h) | Each cell's background as a block of pixels, no glyphs; a minimal check that cells become pixels |
| `PixelBackend` | [pixel_backend.h](../oui/include/oui/graphics/pixel_backend.h) | The base of every graphical backend: paints each frame on a `Canvas` with a `GraphicalPainter` |
| `DesktopWindow`, from `window_backend()` | [window_backend.h](../oui/include/oui/graphics/window_backend.h), `window_backend_sdl.cpp` | A desktop window through SDL3; OUI draws its frame and title bar too |
| `drm_backend()` | [drm_backend.h](../oui/include/oui/graphics/drm_backend.h) | The whole screen through the kernel's mode setting, no display server; Linux |
| `ImageBackend`, from `image_backend()` | [pixel_backend.h](../oui/include/oui/graphics/pixel_backend.h) | Pixels in memory, with a script of events, for tests and pictures; `write_png` saves the canvas |
| `desktop_backend()` | [desktop.h](../oui/include/oui/desktop.h), [graphics/desktop.h](../oui/include/oui/graphics/desktop.h) | A window on OUI's own desktop: pixels in memory shared with it, or cells when the desktop is in a terminal |

The graphical backends live in the `olagarro::OUIGraphics` library, the SDL window in `olagarro::OUIWindow`
(`OLAGARRO_OUI_WINDOW=ON`) and the DRM screen with `OLAGARRO_OUI_DRM=ON`; see [building](building.md).

`PixelBackend` does the common work. Its `size()` is its content area in cells of the cell font, fractional, so the
root reaches its last pixel; `cell_at(pixel)` is where a pixel of the content area is in cells, which is what the
SDL window and the DRM screen give as the pointer's `x` and `y`;
`render` fills the canvas with the style's `desktop` colour, paints the frame with a `GraphicalPainter` limited to the
content area, then calls `decorate` (what the backend draws around it, as the SDL window's frame and title bar) and
`show`, which each backend implements: SDL uploads the canvas as a texture, DRM copies it into a dumb buffer, draws
the pointer on it and flips pages. `present(CellBuffer)` on a pixel backend shows a buffer of cells as it is, in the
cell font.

## The cell buffer

A `CellBuffer` is a width by height grid of `Cell`:

```cpp
struct Cell
{
    char32_t glyph{U' '};
    Color foreground{255, 255, 255};
    Color background{0, 0, 0};
    std::uint8_t attributes{0};
    bool continuation{false};   // the second cell of a wide character
};
```

`CellBufferPainter::draw_text` decodes UTF-8 and places each character by `terminal_cell_width`
([utf8.h](../oui/include/oui/utf8.h)): one cell for most, two for wide ones such as CJK, whose second cell is marked
`continuation`, and none for combining marks. A combining accent after a letter, as NFD text writes it, is composed
into the letter's cell where a precomposed character exists. Characters outside the clip are skipped but still
advance.

`fill_rect` writes spaces with the foreground set to the background too, so a shadow cast over the blank cells later
shows nothing but the shadow's colour. `shade` keeps each cell's character and changes its colours, and clears its
attributes.

## Writing to a terminal

`AnsiTerminalBackend` on POSIX ([terminal_backend_posix.cpp](../oui/src/terminal_backend_posix.cpp)) sets the
terminal up when it starts: raw mode (no echo, no line editing, no signals or flow control, so every key reaches the
program), any-motion SGR mouse reports, modifyOtherKeys (so Ctrl+Shift+letter is told apart), focus reports, the
alternate screen unless `TerminalOptions::use_alternate_screen` is false, no automatic wrap, and the cursor hidden.
It restores all of it on exit, on `SIGHUP` and `SIGTERM`, and after a crash.

`present` compares the new buffer with the one it sent last and writes only what changed:

- for each changed cell, it moves the cursor there (`ESC [ row ; column H`), writes the cell's style (an SGR
  sequence with attributes and colours) and its character;
- it keeps writing the following cells in the same run while they changed too and have the same style, so a changed
  word costs one cursor move and one style;
- `continuation` cells write nothing, since the terminal advanced past them with the wide character;
- at the end, `ESC [ 0 m` and a single write and flush.

The first frame, and any frame after a resize, is written whole. `reset()` forgets the last frame, so the next one is
written whole; `Application` calls it when its content is replaced.

On Windows ([terminal_backend_win32.cpp](../oui/src/terminal_backend_win32.cpp)), with virtual terminal sequences the
output is the same diff; without them, as on the legacy console or with `TerminalOptions::force_legacy`, the whole
buffer is written with `WriteConsoleOutputW` each frame, in console attributes.

## Colours in a terminal

Theme colours are 24-bit RGB. How they reach the screen depends on the terminal; `style_for` in the POSIX backend
picks one of four forms, as `colour_mode` reads the environment
([terminal_backend_helpers.h](../oui/src/terminal_backend_helpers.h) has them):

| Terminal | Form | What arrives |
|----------|------|--------------|
| `COLORTERM` is `truecolor` or `24bit`, the way terminals say they take 24-bit colours | `style` | The exact RGB: `38;2;r;g;b` and `48;2;r;g;b` |
| `TERM` contains `256color` and `COLORTERM` says nothing of 24 bits | `palette_style` | The nearest of xterm's 256, `38;5;n`: of its 6 by 6 by 6 cube or of its 24 greys, from `palette_index`. Some of these terminals, as macOS's Terminal, show 24-bit colours wrong |
| Any other | `reduced_style` | The nearest of the 16 colours of a console, as EGA and VGA showed them, from `ansi_color`: the colours themes for characters are made of come out as themselves, the brown and the dark grey included |
| The Linux console, or `OUI_LINUX_CONSOLE=1` | `console_style` | 16 foregrounds but only the 8 dark backgrounds, which is all the console shows; a bright white background becomes cyan |

The Linux console shows a bright background as its dark version, which turns white into light grey, the colour
dialogs and menus are made of: a focused control marked with white would vanish into them, so white becomes cyan, the
highlight of DOS-era text interfaces. The console is recognised by asking the kernel for its font's characters
(`console_font_glyphs`), which also tells which characters it cannot draw: a button's half-block shadow (`▀`, `▄`)
becomes a space there, as a whole block in its place reads as a stain. `OUI_LINUX_CONSOLE=1` makes any terminal draw
as the console would, to check a program without switching to one.

The legacy Windows console takes four-bit attributes: the nearest of the same sixteen, `windows_console_attribute`
with blue as its lowest bit as the console orders them, bold adding intensity, and a foreground equal to its
background made visible (`with_visible_foreground`). Windows with virtual
terminal sequences always gets the exact RGB.

So a colour from the sixteen of a text console (`TurboCTheme`'s constants, or those at the top of
[terminal_themes.cpp](../oui/src/terminal_themes.cpp)) shows as itself everywhere, which is why the
[terminal themes](themes.md#terminal-themes-and-the-sixteen-colours) keep to them.

## Pixels: the graphical painter

`GraphicalPainter` paints on a `Canvas` ([canvas.h](../oui/include/oui/graphics/canvas.h)): pixels in memory as
`0x00RRGGBB`, with a clip rectangle that every operation respects, and fills, blends, antialiased polygons and lines,
copies, and glyphs drawn from coverage bitmaps.

A cell is as wide as the cell font's `M` and as high as its line (`GraphicalPainter::cell_size`), and every row is
that high, so a rectangle in pixels is its cells times that size, from the top left of the area the painter was
given. Each edge is rounded to the nearest pixel on its own, so rectangles that meet in cells meet in pixels, with no
gap and no overlap, wherever between cells they fall. A scrollbar's thumb, in whole cells from the whole cell its
track starts in as a terminal shows it, is drawn as far from the track's own start.

- **Cells.** `draw_text` fills each cell with the background and draws the character centred in it from the cell
  font, combining accents composed as in the cell buffer. Bold draws the glyph twice a pixel apart, underline is a
  line two pixels under the baseline, and dim mixes the foreground towards the background.
- **Interface text.** `draw_label`, `draw_label_runs` and `draw_mnemonic_label` at a position fill the cells the text
  takes, `shown_width(text)` of them, and set the text in the interface font, vertically centred, kept
  `GraphicalStyle::text_margin` pixels from the edge it is aligned to. `draw_label` in an area sets it the same way
  within the area, fills the background only behind the text and its margins, and draws nothing outside the area.
- **Elements.** Windows, buttons, tabs, check boxes, scrollbars, bars, menus and fields are bevels (a face with a
  light edge at the top and left and a shadow edge at the bottom and right, or the other way round when sunken) in the
  `GraphicalStyle`'s shades, with a focus ring in its `focus` colour. Shapes are designed for cells 24 pixels high and
  scaled with the cell (`unit()`), so they grow with [zoom](#zoom).

The graphical painter takes its shapes' colours from the style and reads only a few of the theme's; see
[graphical themes](themes.md#graphical-themes).

## Fonts

A graphical style writes with three fonts ([font.h](../oui/include/oui/graphics/font.h)), gathered in a `FontSet`:

| Font | Built in | For |
|------|----------|-----|
| Cell font | Spleen, a PC Screen Font (`.psfu`) bitmap font: 8x16, 12x24 and 16x32, and the last two doubled; `psf_font` | Cells: code, tables, terminals, everything on the grid |
| Interface font | Jersey 10 or Noto Sans, as the style's `interface_font` says, TrueType, rendered by stb_truetype; `truetype_font` | Interface text: labels, titles, menus, buttons |
| Symbol font | Drawn as shapes, `symbol_font(cell_width, cell_height, ascent)` | Arrows, triangles and squares the other two lack |

The interface font falls back to the cell font for a character it lacks, as a box line, and to the symbols for an
arrow, on the same baseline (`FontSet::interface_glyph`); a tab in interface text is a space. The fonts and their
licences are in [oui/fonts](../oui/fonts) and are built into `oui_graphics` with `olagarro_embed_files`, so a
program carries them. `default_cell_font()` is Spleen 12x24 and `default_interface_font()` Jersey 10 at 24 pixels.

stb_truetype is compiled as it comes in a file of its own, `stb_truetype_api.cpp`, behind the small interface of
[stb_truetype_api.h](../oui/src/graphics/stb_truetype_api.h), so the project's warnings, which are errors, stay off
third-party code.

## Text metrics and proportional text

Layout works in cells, but interface text in a proportional font takes fewer pixels than its characters' cells. A
text field must also put its cursor between the right two letters. `TextMetrics`
([text_metrics.h](../oui/include/oui/text_metrics.h)) answers such questions for whatever shows the application:

| Method | Terminal | Graphical style |
|--------|----------|-----------------|
| `cell_width()` | 1 | the cell's width in pixels |
| `column_step()`, `row_step()` | 1: a whole cell | a pixel: 1 / the cell's width, 1 / its height |
| `cell_pixels()` | none | the cell's size in pixels |
| `width(text)` | columns | pixels of the interface font |
| `field_padding()` | 0 | half a cell |
| `popup_frame_rows()` | 1: a frame line above and below | 0: a thin edge inside the entries' rows |
| `panel_gap()` | 0: panels meet, each in its colour | 1 |
| `window_top_rows()` | 1: the frame line, the title in it | 2: the title bar and a row under it |
| `window_side_columns()` | 2: the frame and a cell beside it | 1 |
| `window_close_button()` | false | true |
| `byte_at(text, position, through)` | the character in that column | the nearer edge of the character there |

`text_metrics()` returns the current ones: the terminal's unless a backend set its own. `PixelBackend` sets
`PixelTextMetrics` with `set_text_metrics` while it lives, and again whenever its fonts change size; its destructor
puts the terminal's back. It is a global because widgets measure in `measure()` and `arrange()`, which take no
painter.

Widgets use them through [text_layout.h](../oui/include/oui/text_layout.h):

- `text_width(text)`: columns, always, for text on the grid.
- `shown_width(text)`: the cells interface text takes as it shows, rounded up to whole cells, with the field padding
  on both sides: its columns in a terminal, fewer in a proportional font. What a painter fills behind text drawn at a
  position.

and through `text_metrics()` itself:

- `label_width(text)`: the cells interface text takes as a label shows it, with its margins: its columns in a
  terminal; in a window its pixels and `text_margin` on each side, up to the column step, so it may end between cells.
  What a label, a button, a check box, a combo box, a tab, a menu's entries and the status bar's keys measure; such a
  widget draws its text with `draw_label` in its bounds. Nothing for no text.
- `wrap_text(text, width)` and `fit_text(text, width)`: lines that fit, broken at spaces, or the text cut with `~`,
  both measured with `text_metrics()`.

So in a window a label is narrower than its characters, a button with text is as wide as its text and margins (its
raised face needs no cells of frame and shadow), a menu needs no frame rows, and a window has a title bar of
two rows; in a terminal the same code gives the Turbo Vision shapes.

## Zoom

A pixel backend has five sizes (`zoom_table` in [pixel_backend.cpp](../oui/src/graphics/pixel_backend.cpp)): cell
fonts 16, 24, 32, 48 and 64 pixels high, with the interface font as high as each: Jersey 10's line runs from its
highest accent to its lowest tail, so it fills the row without spilling, and menus look as big as the text beside
them. 24 is the default (`default_zoom()` is 1). `set_zoom` loads the fonts at the new size and calls
`fonts_changed`; the SDL window lays itself out again there and sends a `ResizeEvent`, since the same pixels now hold
another number of cells. Every frame arranges the widgets in `size()` anyway, so the layout follows on the next one.

`Application` offers zoom where the backend `zooms()`: the commands `zoom.in` (Ctrl++ and Ctrl+=), `zoom.out`
(Ctrl+-) and `zoom.reset` (Ctrl+0), titled **Zoom In**, **Zoom Out** and **Normal Size**, and Ctrl with the mouse
wheel unless `set_wheel_zooms(false)`. `on_zoomed` tells the program, as OUI Code does to keep the level in its
`window_zoom` setting. A terminal does not zoom: it has its own keys for that.

## Writing a widget's paint()

```cpp
void Button::paint(Painter& painter, const Theme& theme) const
{
    const auto shown{strip_mnemonic(m_text)};
    painter.draw_button(bounds(),
                        {shown, m_pressed, is_focused(), m_shadow_visible, m_compact, m_default,
                         mnemonic_offset(m_text), resolve(m_background, theme, theme.dialog_background), is_enabled()},
                        theme);
}
```

What a `paint()` does, from OUI's own widgets:

- **It is `const` and draws only.** State changes in event handlers; `paint` reads state and draws it. Painting
  again must give the same picture.
- **It draws in `bounds()`**, in cells, which `arrange` set. A container clips each child to its `paint_bounds()`
  before painting it (`Container::paint`), so a child cannot spill over its neighbours; a widget that needs a tighter
  clip, as a table for each cell, pushes and pops its own.
- **It asks for elements where there is one.** A button calls `draw_button`, a scroll view `draw_scrollbar`, a label
  `draw_label`: then it looks right in every style, a terminal's and a graphical one, with no code for each. Plain
  `draw_text` and `fill_rect` are for content on the grid: code, tables, a terminal's screen.
- **Interface text goes through `draw_label`** (or `draw_mnemonic_label` with an Alt letter, `draw_label_runs` for
  mixed styles), not `draw_text`, so a graphical style can set it in the proportional font; a widget sized by its
  text measures it with `text_metrics().label_width` and draws it with `draw_label` in an area, not `text_width`.
- **Colours come from the theme**, or from a `ColorRef` resolved with it, never written in: a widget with a fixed
  colour would ignore every theme. See [ColorRef](themes.md#colorref-colours-resolved-when-painting).
- **It paints its background** where it has one; what is under it is whatever was painted before.
- **It asks for a frame when its state changes**, not by painting: event handlers return true when they handled an
  event, and the application paints after events, timers and posted work. Changes from elsewhere go through
  `Application::post`, or happen in a timer.

A label, for instance, picks its colours from the theme, the selection's when its parent says it is selected, and
draws its text as a label, with the mnemonic when it names a field ([label.cpp](../oui/src/label.cpp)):

```cpp
const auto foreground{m_color    ? m_color->resolve(theme)
                      : selected ? theme.selection_foreground
                                 : theme.dialog_foreground};
TextStyle style{foreground,
                selected ? theme.selection_background : resolve(m_background, theme, theme.dialog_background)};
...
painter.draw_mnemonic_label(at, shown, *letter, style, theme.label_shortcut_foreground);
```

## Rendering in tests

Tests paint without a terminal. `ScriptedBackend` (default 80 by 25 cells) hands out its events, keeps the last frame
(`last_frame()`), and every frame with `set_keep_frames(true)`; a test reads the cells, their characters and colours,
as the contrast tests in `tests/oui/contrast_test.cpp` do. `set_one_event_per_frame(true)` hands out one event per
frame and only once something was drawn, as a person reacting to the screen; `set_on_drained` runs when the script
runs out. `ImageBackend` does the same in pixels with the graphical painter, and `write_png` saves a frame to look
at. [Testing](testing.md) describes the tests as a whole.
