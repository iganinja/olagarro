# Paint

Painting without OUI: the pieces [OUI Paint](https://github.com/iganinja/oui_paint) paints with, shared with ouivi,
which paints drawings in its viewer. Namespace `olagarro::paint`, CMake target `olagarro::Paint`, in the build tree and
from an installed package alike. It depends on [Concurrency](concurrency.md) and [Effects](effects.md), and nothing
else: no OUI, no files, no scripts. What a program has of its own, OUI Paint's documents of several layers and its
files, ouivi's tracks, is built on top of it. Its panels on OUI are a target of their own, `olagarro::PaintPanels`
([Panels](#panels)).

- [Pixels](#pixels)
- [The canvas](#the-canvas)
- [Undo](#undo)
- [Files](#files)
- [Blend modes and colours](#blend-modes-and-colours)
- [Selections](#selections)
- [Tools](#tools)
- [The pieces the tools are made of](#the-pieces-the-tools-are-made-of)
- [Panels](#panels)

## Pixels

```cpp
#include <paint/image.h>

using namespace olagarro;
paint::Image image{paint::PixelFormat::rgba16, 640, 480};  // see-through black
image.set_pixel(10, 20, paint::Rgba16{{65535, 0, 0, 65535}});
const auto small{paint::converted(image, paint::PixelFormat::rgba8)};
```

`PixelFormat` (`paint/pixel_format.h`) is grey and alpha or red, green, blue and alpha, of 8 or 16 bits a channel:
`gray_alpha8`, `rgba8`, `gray_alpha16`, `rgba16`. Colour is straight, not multiplied by alpha, in the screen's curve,
as files keep it. Each has its pixel type, `GrayAlpha8`, `Rgba8`, `GrayAlpha16` and `Rgba16`, its channels in
`value`; `visit_format(format, action)` calls `action` with a `FormatTag` of the one at hand, so a piece written once
as a template runs on any.

`Image` (`paint/image.h`) is pixels of one format in one block, row after row: what files are read into and written
from. `converted` takes it to another format: 8 bits become 16 by repeating the byte, 16 become 8 rounded to the
nearest, colour becomes grey by its luma and grey colour with the same value in all three.

## The canvas

```cpp
#include <paint/canvas.h>

paint::Canvas canvas{paint::PixelFormat::rgba8, 1920, 1080};
canvas.write(image, 100, 50);                       // its top left corner at 100, 50
const auto part{canvas.read(0, 0, 300, 300)};       // see-through where nothing is
```

A `Canvas` is one picture to paint on, of a size and a format, kept in `Tile`s of 256 by 256 pixels (`tile_size`), at
`TileIndex`es counted in tiles from its top left corner. A tile it does not have is see-through black, and it keeps
none that holds only that: an empty canvas costs nothing, and a drawing with a little on it the tiles drawn on.
`write` makes no tile where it writes nothing over nothing, and forgets one it leaves all zero; a tool writing pixels
itself, through `writable_tile`, calls `drop_empty_tiles` when it is done.

Tiles are shared until written to. A copy of a canvas costs a pointer a tile; `writable_tile` copies a tile someone
else holds before giving it. Nothing stops at the canvas's edges, as a piece being moved may pass them: its size is
what is read when the canvas becomes a file.

## Undo

```cpp
#include <paint/tile_change.h>

const auto before{canvas};                          // a pointer a tile
// ... a stroke paints the canvas ...
const auto change{paint::tile_change(before, canvas)};
change.undo(canvas);                                // as it was
change.redo(canvas);                                // as the stroke left it
```

A `TileChange` is what one step did to a canvas: `before` and `after`, each the tiles it changed with what they held,
none where a tile was or became see-through. `undo` puts the tiles back as they were, `redo` as the step left them,
and `tiles()` says which to draw again. `tile_change` finds it from a copy kept as the step began and the canvas as it
ended: the tiles they still share are passed over, and those copied to be written but left as they were count as no
change, so a step that changed nothing is `empty()`. A program's history keeps one a step: OUI Paint's beside its
layers, ouivi's beside its edits of the project.

## Files

```cpp
const auto canvas{paint::from_rgba8(width, height, pixels)};   // as stb_image gives them
const auto image{paint::to_rgba8(canvas)};                     // image.bytes, as stb_image_write takes them
```

`from_rgba8` makes a canvas, of 8-bit RGBA unless another format is asked for, from a block of 8-bit RGBA pixels, the
see-through tiles not kept; `to_rgba8` gives the canvas's width by its height in 8-bit RGBA. Reading and writing the
files is the program's.

## Blend modes and colours

`paint/blend.h` has Paint.NET's modes, `BlendMode` (normal, multiply, additive, colour burn and dodge, reflect, glow,
overlay, difference, negation, lighten, darken, screen, xor), with `all_blend_modes()` in Paint.NET's order and
`blend_mode_name`. `blend(below, above, opacity, mode)` lays one pixel over another of any format as the W3C's
compositing does: over nothing, the pixel above shows as it is whatever the mode.

`Color` (`paint/color.h`) is red, green, blue and alpha from 0 to 1, whatever the depth; `to_hex` writes `#rrggbb`, or
`#rrggbbaa` when it is not solid, and `from_hex` reads either.

A `Palette` (`paint/palette.h`) is a name and colours to paint with; `Palette::standard()` is the colour panel's greys
and colours, named "Default". `to_gpl` writes it as GIMP's `.gpl` text and `palette_from_gpl` reads that, GIMP's own
files too. The format has no alpha: a colour not solid goes under its `#rrggbbaa` as the colour's name, which gives the
alpha back when read, and one wholly see-through as black named `transparent`. Reading and writing the files is the
program's, or the panels' library's.

## Selections

```cpp
#include <paint/selection.h>

paint::Selection selection{1920, 1080};             // nothing selected: the whole image
selection.add_ellipse(100, 100, 400, 300, paint::SelectionMode::replace);
selection.add_polygon({{10, 10}, {60, 10}, {10, 60}}, paint::SelectionMode::add);
const auto amount{selection.at(120, 200)};          // 0 to 255
```

A `Selection` is how much each pixel of an image is selected, from 0 to 255, kept as a grey canvas (`mask()`), so
keeping it for undo costs what a canvas does. With nothing selected, `active()` is false and tools work on the whole
image. Rectangles, ellipses, polygons (a lasso, inside by the even-odd rule) and a coverage for every pixel (a magic
wand's region) are combined with what is selected by a `SelectionMode`: replace, add, subtract, intersect or invert.
Ellipses and polygons have a soft edge a pixel wide: a pixel the edge cuts through the middle of is half selected.
`bounds()` is the smallest `PixelArea` holding everything selected, `offset` moves it by whole pixels, what leaves the
image lost. Points are `Position`s (`paint/geometry.h`) in the image's pixels, (0.5, 0.5) the middle of the first.

## Tools

```cpp
#include <paint/tool_session.h>

paint::Canvas canvas{paint::PixelFormat::rgba8, 1920, 1080};
paint::Selection selection{1920, 1080};
paint::ToolSession session{canvas, selection};
session.set_tool(paint::Tool::brush);
session.options().brush.width = 12.0;
session.options().primary = {1.0, 0.0, 0.0, 1.0};

paint::ToolInput input;
input.at = {100.5, 200.5};                          // in the canvas's pixels
session.press(input);
input.at = {300.5, 220.5};
session.drag(input);
session.release(input);
for(auto& step : session.take_steps())              // one: the stroke
{
    history.push_back(std::move(step));             // step.change.undo(canvas) takes it back
}
```

A `ToolSession` uses one tool at a time on a canvas and its selection, which outlive it. It is given the pointer as
`ToolInput`s: where it is in the canvas's own pixels, the right button (`secondary`), Shift, Ctrl and Alt, and a pen's
`pressure`, from 0 to 1. Whatever shows the canvas, a program's view of it at some zoom or a video editor's frame with
the drawing moved and scaled in it, only turns its pointer into those pixels; the session knows nothing of views. It
hears of `press`, `drag` (with the button held: every move for the tools that trace the pointer, the last of a frame is
enough for the others), `release`, and `hover`, the pointer moving with no button, which only the arc hears of.

`Tool` names what it can do, `tool_id` its name in OUI Paint's settings ("brush", "bucket" for the fill):

- `brush`, `pencil` (one pixel, hard, no antialiasing) and `eraser` paint as the pointer goes, each segment between two
  moves a line however fast the drag: a `Stroke` (below), whose width follows the pressure.
- `fill` fills from the pixel pressed what is within the fill's tolerance of it, joined to it or, `global`, anywhere.
- `gradient`, `line` and `shapes` are dragged from where they are pressed: until released they are only shown, in the
  overlay's `preview`, and drawn into the canvas on release. With Shift a line keeps to steps of 15 degrees and a shape
  is a square or a circle. A line drawn waits to be bent: its ends and two handles a third and two thirds along it are
  the overlay's `handles`, a press on one drags it, and a press elsewhere, another tool, or `finish()` leaves it as it
  is, one step from the first press. The right button swaps the colours.
- `arc` is drawn as with a compass (`ArcGesture`): a click puts its centre, a second its radius and where it starts, the
  pointer then turns it, and a third click draws it.
- `rectangle_select`, `ellipse_select` and `lasso_select` select as they are dragged, `magic_wand` what a fill would
  reach. The mode is Paint.NET's: Ctrl adds, Alt takes away, the right button with Alt intersects and with Ctrl
  inverts. A click without dragging selects nothing.
- `color_picker` makes the pixel under the pointer the primary colour, or the secondary with the right button.
- `move_pixels` lifts what is selected (the whole canvas when nothing is) and moves it by whole pixels, turns it round
  its middle with the right button (by 15 degrees with Shift), or scales it by one of its eight handles (corners, then
  edges), the opposite one staying put. What floats is laid over the hole it left at each move, so the canvas always
  shows it, and is put down when another tool is used. `move_selection` moves the selection's outline only.

Two uses need no pointer, each a step of its own after what is under way is put down: `erase_selection()` makes the
selected pixels see-through, as Paint.NET's Delete does, and `deselect()` selects nothing, as its Ctrl+D does; neither
does anything while nothing is selected. `flip_selection(horizontal)` and `turn_selection(quarter_turns)` (1 clockwise,
2 half round, 3 counter-clockwise), as OUI Paint's Flip and Rotate Selection, lift what is selected as `move_pixels`
does, unless it floats already, and flip or turn it round its middle with the selection, a step each; it is left
floating, so those that follow keep its pixels exact, and nothing is done while nothing is selected or floats.

`ToolOptions` are what they paint with: `brush` (width, hardness, antialiasing), `fill` (tolerance, global), `shape`
(rectangle, rounded rectangle or ellipse; outline, filled, or both), `line` (arrowheads), `gradient` and its
`gradient_transparency`, and the `primary` and `secondary` colours. A program shows and changes them through
`options()`; the picker changes the colours itself.

Each use of a tool that changed something is a `ToolStep`, taken with `take_steps()`: the tool, the `TileChange` it made
to the canvas, and a `SelectionChange`, the selection's mask before and after, for the tools that select or move. A
program's history keeps them and undoes one by `change.undo(canvas)` and putting `selection->before` back.
`take_changed()` gives the tiles changed since last asked, to draw again. `overlay()` is what to draw over the canvas
while a tool works, in its pixels: the `preview` to show in its place, `lines` (the selection's outline, a lasso being
drawn, an arc's radius) and `handles`; `set_handle_reach` says how near a handle a press takes it, in the canvas's
pixels, so a program sets it from its zoom. `selection_outline` is the edge of what is selected half or more, as lines
between pixels.

`busy()` says whether something is under way: a button held, a line still to bend, an arc between its clicks, pixels
floating. `finish()` leaves it as it is, an arc between its clicks left out; `cancel()`, as Esc, leaves out what is not
yet a step, the canvas as it was.

## The pieces the tools are made of

The tools are OUI Paint's, which a program may also use alone, as OUI Paint does for what its own commands need:

- `paint/stroke.h`: a `Stroke` paints, erases, clones (the Clone Stamp, `StrokeSource`'s offset) or recolours (one
  colour, within a tolerance) along the points given to `line_to`, each pixel taking the most the stroke ever covered
  it, so a see-through colour does not build up where it crosses itself. `fill_selected`, `erase_selected`,
  `lift_selected` and `lay_over` do what Backspace, Delete and a paste do with the selection.
- `paint/fill.h`: `fill_region` is the pixels a fill reaches in an image, compared with the pixel pressed; `bucket_fill`
  fills them on a canvas comparing its own pixels, and `fill_reached` fills what a region, as of a program's whole image
  of several layers, reaches.
- `paint/gradient.h`: Paint.NET's five gradients, and between two indices of a grey canvas, dithered.
- `paint/shapes.h`: rectangles, rounded ones and ellipses, lines with arrowheads, curves through four points, and arcs.
- `paint/arc.h`: `ArcGesture`, the three clicks of an arc.
- `paint/floating.h`: a `FloatingPiece` lifted from a canvas, the `PieceTransform` that moves, turns, scales and mirrors
  it, and `place_piece`, which lays it down with its selection, keeping every pixel exactly when it only moves by whole
  pixels, mirrors or turns by quarter turns.
- `paint/transform.h`: `Affine`, and images flipped, turned, resized and transformed, by nearest, bilinear, bicubic or
  supersampling `Resampling`.
- `paint/color_math.h`: colours to hue, saturation and value and back.

## Panels

```cpp
#include <paint/panels/color_panel.h>
#include <paint/panels/tool_options_bar.h>
#include <paint/panels/tool_panel.h>

paint::Icons icons;                                 // outlives the panels that show them
auto tools{std::make_unique<paint::ToolPanel>(icons)};
auto options{std::make_unique<paint::ToolOptionsBar>(session.options())};
auto colors{std::make_unique<paint::ColorPanel>()};
tools->on_picked([&](const std::string&)
                 {
                     if(const auto tool{tools->selected_tool()})
                     {
                         session.set_tool(*tool);
                         options->show_for(*tool);
                     }
                 });
options->on_changed([&](paint::ToolOptions chosen)
                    {
                        chosen.primary = colors->primary();
                        chosen.secondary = colors->secondary();
                        session.options() = chosen;
                    });
paint::add_tool_commands(application.commands(), *tools);   // B, P, E, F, G, O, S, K, M
paint::add_color_commands(application.commands(), *colors); // X swaps, C edits the other
```

`olagarro::PaintPanels` is OUI Paint's panels, moved as they were, on OUI and OUIGraphics: it is for a program that
shows a `ToolSession` and wants Paint.NET's way of choosing what it paints with.

- `ToolPanel` (`paint/panels/tool_panel.h`) is the tools as icon buttons, two to a row, as Paint.NET's Tools window; a
  click on one, or `press_key`, picks it, and `on_picked` hears the tool's id. `paint_tool_buttons()` are Paint's tools
  in OUI Paint's order, each with its icon and Paint.NET's key: B brush, P pencil, E eraser, F fill, G gradient, O the
  line, shapes and arc, S the four selections, K the picker, M the two moves; a key pressed again goes to the next tool
  that has it. `set_scale` draws the buttons and their icons bigger than the cells, as a program whose painting wants
  larger buttons than its interface does; a terminal keeps 1. A program with tools of its own (OUI Paint's zoom, pan,
  clone stamp, recolor and text) gives the panel
  its own list, Paint's among them, and `selected_tool()` is none while one of its own is picked.
  `add_tool_commands` registers each key as a command bound to it ("tool.key_b"), so it works wherever the program's
  commands do.
- `ToolOptionsBar` (`paint/panels/tool_options_bar.h`) is the bar of the tool in use's options, as Paint.NET's tool
  bar: the brush's width (a number and a slider), hardness and antialiasing, the fill's mode and tolerance, the
  gradient's kind and mode, the line's arrowheads, the shape and its style. `show_for` shows those a tool uses;
  `options()` is a `ToolOptions` as the bar shows them, `on_changed` hears each change, `set_options` shows others
  without telling, and `change_width` steps the width, as `[` and `]` do in OUI Paint. The colours are the colour
  panel's: the bar keeps those it was given. A program's own options go at the end of `bar()`.
- `ColorPanel` (`paint/panels/color_panel.h`) is the primary and secondary colours, two squares one over the other,
  and a palette of greys and colours under them: a click on the palette sets the colour being edited, the right button
  the other, and `on_edit` hears a click on a square, for the program to ask for a colour. `add_color_commands` binds
  X to `swap` and C to `toggle_editing`, which chooses the colour the palette's left button sets. `set_scale` draws it
  bigger, as the tools; `set_palette_columns` puts more colours to a palette row, each as much smaller, so a long
  palette fits under the tools of a short window.
  The palette is a `Palette` (`palette()`, `set_palette`). Given a `PaletteLibrary` (`set_library`) and what to ask
  with (`set_asks`, `palette_asks(application, open_menu)` for Paint's colour dialog and OUI's prompt, message boxes
  and file dialog), the panel edits it: two buttons under the squares, drawn with `set_icons`' "Save" and "Load"
  (letters without them), save it as a name (asked, its own offered and saved over without asking) and open the
  library's palettes to choose one or import a `.gpl`, their tooltips saying its name, with "*" while
  `palette_modified()`; a double click on a colour asks for another in its place, a "+" after the last adds the
  primary colour, a colour dragged away from the panel goes and dragged onto another moves there, and a right click
  away from the colours opens a menu:
  *Palettes* (the library's, the one in use ticked), *Save*, *Save as…*, *Rename…*, *Remove*, *Import a .gpl…* and
  *Add the primary colour*. `on_palette_changed` hears each change the panel makes, not those `set_palette` makes.
- `PaletteLibrary` (`paint/panels/palette_library.h`) keeps palettes by name as the `.gpl` files of a folder,
  `default_folder()` being `olagarro/palettes` in the user's configuration folder (`oui::user_settings_folder`), the
  same for every program on Olagarro: a palette saved in one is there in the others. It lists (`names`), loads, saves
  (a name's file replaced), renames and removes them; "Default" is always first and cannot be replaced, renamed or
  removed.
- `ask_color` (`paint/panels/color_dialog.h`) asks for a colour as Paint.NET's Colors window does: a square of
  saturation and value, a bar of hue, red, green, blue and alpha, hue, saturation and value, hexadecimal, and grey as a
  16-bit value for heightmaps. Unlike `oui::ask_color`, it has alpha and keeps colours deeper than 8 bits.
- `Icons` (`paint/panels/icons.h`) draws the icons the panels show, built in (`paint/panels/icons`: the Godot editor's,
  under the MIT license, and OUI Paint's own in their style), as Godot draws them for a dark interface or as its light
  themes recolour them; `add` gives a program's own.

The panels' texts are translated (`oui::translate`, contexts `Tools`, `Commands`, `Dialogs` and `Palettes`) by catalogs of their own
in `paint/translations`, built in as `panel_translations()` (`paint/panels/panel_translations.h`), which a program gives
`oui::install_language` before its own.
