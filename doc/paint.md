# Paint

Painting without OUI: the pieces [OUI Paint](https://github.com/iganinja/oui_paint) paints with, shared with ouivi,
which paints drawings in its viewer. Namespace `olagarro::paint`, CMake target `olagarro::Paint`, in the build tree and
from an installed package alike. It depends on [Concurrency](concurrency.md) and [Effects](effects.md), and nothing
else: no OUI, no files, no scripts. What a program has of its own, OUI Paint's documents of several layers and its
files, ouivi's tracks, is built on top of it.

- [Pixels](#pixels)
- [The canvas](#the-canvas)
- [Undo](#undo)
- [Files](#files)
- [Blend modes and colours](#blend-modes-and-colours)
- [Selections](#selections)

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
