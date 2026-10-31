# Olagarro for an AI, top to bottom

What an AI, or anyone in a hurry, needs to write a program with Olagarro or change it: what there is, how to bring it
into a project, how an OUI program is put together, the rules that are easy to break, and where each detail is. It is
a map; each section ends at the document that has the whole of it.

- [What there is](#what-there-is)
- [Bringing it into a project](#bringing-it-into-a-project)
- [An OUI program](#an-oui-program)
- [Widgets, ownership and layout](#widgets-ownership-and-layout)
- [Cells and pixels](#cells-and-pixels)
- [Events, focus and commands](#events-focus-and-commands)
- [Dialogs and descriptions](#dialogs-and-descriptions)
- [The terminal and the window](#the-terminal-and-the-window)
- [Testing](#testing)
- [Scripts](#scripts)
- [Rules of the house](#rules-of-the-house)
- [Mistakes easy to make](#mistakes-easy-to-make)
- [Where everything is](#where-everything-is)

## What there is

C++17 libraries, each a CMake target named `olagarro::<Component>`:

| Target | For |
|--------|-----|
| `olagarro::OUI` | The UI toolkit: widgets, layout, events, commands, menus, dialogs, text editing, files, processes, pseudo-terminals, translations, settings, and the terminal backend. Namespace `olagarro::oui`. |
| `olagarro::OUIGraphics` | OUI in pixels: the graphical painter and its styles, the fonts built in, the full-screen DRM backend. |
| `olagarro::OUIWindow` | A desktop window and sound through SDL3, linked statically. Only with `OLAGARRO_OUI_WINDOW=ON`; not installed. |
| `olagarro::OUIScript` | JavaScript, run by QuickJS-ng, for programs that take scripts. Namespace `olagarro::oui::script`. |
| `olagarro::SignalSlot` | Header-only signals and slots. |
| `olagarro::ByteStream` | Binary serialisation and files. |
| `olagarro::Tasks`, `olagarro::CTTasks` | Tasks composed at run time, and at compile time with no heap, for games and animations. |
| `olagarro::Concurrency` | A task pool with cancelling, "latest wins" requests and `concurrent_for`. |
| `olagarro::Effects` | The pieces of picture and sound effects, on the task pool. |

OUI is most of it. A program made with it runs the same in a terminal, over SSH too, in a window of the desktop, on
the whole screen of a Linux console, and on OUI's own desktop. Programs larger than the examples here have repositories
of their own: [OUI Code](https://github.com/iganinja/oui_code) (an editor, oui_run and oui_calc) and [OUI
Paint](https://github.com/iganinja/oui_paint) (an image editor). Both are good code to copy from.

[Overview](overview.md) has every library's dependencies and the repository's layout.

## Bringing it into a project

Olagarro makes no releases: a version is a tag, `v0.4.0` and on, whose number is the one `project()` gives in
`CMakeLists.txt`. Its minor number goes up when its interface changes in ways that break programs, which in 0.x it
does. It is built from source with the program; there are no prebuilt libraries, as a C++ library's binary serves only
the compiler and standard library it was made with.

**As a submodule**, the way OUI Code and OUI Paint do it, pinned to a commit:

```sh
git submodule add https://github.com/iganinja/olagarro.git third_party/olagarro
git -C third_party/olagarro checkout v0.4.0
```

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_program LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

set(OLAGARRO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OLAGARRO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# ON for a program that also opens in a window; it fetches SDL3 and builds it in.
set(OLAGARRO_OUI_WINDOW OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/olagarro EXCLUDE_FROM_ALL)

add_executable(my_program main.cpp)
target_link_libraries(my_program PRIVATE olagarro::OUI)
```

**With FetchContent**, the same, with no submodule:

```cmake
include(FetchContent)
set(OLAGARRO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OLAGARRO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(olagarro GIT_REPOSITORY https://github.com/iganinja/olagarro.git GIT_TAG v0.4.0)
FetchContent_MakeAvailable(olagarro)
```

**Installed**, with `cmake --install build --prefix …` and `find_package(Olagarro CONFIG REQUIRED COMPONENTS OUI)`;
the window module is not installed, so a program that opens a window takes one of the two ways above.

Brought in by either of the first two, `olagarro_embed_files` is there too: it builds files into a target, compressed,
as a function that returns their contents (`.oui` descriptions, scripts, catalogs, help, fonts), so a program is one
file with nothing beside it.

Olagarro's own build, its options and Windows: [Building](building.md).

## An OUI program

```cpp
#include <oui/oui.h>

#include <memory>
#include <string>
#include <utility>

using namespace olagarro;

int main()
{
    oui::Application application;  // on the terminal it was started in

    auto window{oui::window("Greeter")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& name{oui::line_edit(content)};
    name.set_placeholder("Who to greet");
    auto& greeting{oui::label("", content)};
    oui::button("&Greet", content, [&] { greeting.set_text("Hello, " + std::string{name.text()} + "!"); });

    application.set_window(std::move(window));  // closing the window quits
    application.focus(name);
    return application.exec();
}
```

`exec()` runs frames until `quit()`: timers that are due, work posted from other threads (`post`, the one call
another thread may make), the backend's events, then a render if anything changed. Idle, it sleeps in the backend and
uses no processor. `start_timer(interval, callback)` returns a move-only `Timer` that lives as long as the object.

[Layout and events](oui_layout.md) is the whole of OUI for C++, with a longer complete program at its end.

## Widgets, ownership and layout

- A widget is never copied or moved: it is made on the heap and owned by a `std::unique_ptr`, by its container once
  added, by the application at the top. Each kind has two factories: `oui::label("x")` gives the `unique_ptr`, and
  `oui::label("x", container)` makes it inside the container and gives a reference. `Container::add` also returns the
  child as what it is.
- A reference to a child is good while the child is in the tree. A callback that may outlive the widget holds a
  `WidgetRef<Kind>`, whose `get()` gives nothing once it is gone.
- A widget of one's own derives from `Widget` (or `Container`) and overrides `measure` (required), `arrange`, `paint`
  (const: what paint changes is `mutable`), `handle_event` (true when used) and `is_focusable`.
- Layout runs before every paint: `measure(Constraints)` says what a widget wants, under maximums only;
  `arrange(Rect)` gives it its place.
- The containers: `row` and `column` (each child its measure along the line, all the other way; no stretching),
  `grid` with `Length` columns and rows (`content`, `cells`, `pixels`, `percent`, `fill`, `flex`), `dynamic_grid`,
  `dock_layout` (strips against the edges, `Dock::fill` for the rest), `split_view`, `tab_view`, `scroll_view`,
  `panel`, `vspacer`/`hspacer`, and `window`, which lays out its first child only.
- `TextEdit`, `DockLayout`, `SplitView` and `StatusBar` take all they are offered: in a `Column` they leave nothing
  after them, so they go in a `flex` grid row, a dock's `fill` or a split view. A field that should take the width
  left goes in a grid's `fill` column, not in a row.
- `set_visible(false)` hides a widget but rows, columns and grids keep its room; `set_enabled(false)` dims it and
  makes it answer nothing, and what is inside it too.

The ready-made widgets, each in a header of its own in `oui/include/oui`, are listed in [Layout and
events](oui_layout.md#ready-made-widgets).

## Cells and pixels

Geometry is in cells, columns and rows, as `double`s (`Point`, `Size`, `Rect`, `Insets`, `Constraints`, a
`MouseEvent`'s `x` and `y`). How fine a position may be is the backend's step (`TextMetrics::column_step()`,
`row_step()`): a whole cell in a terminal, so a terminal lays out in whole cells as it always did; a pixel in a window,
where a cell is the cell font's size (12×24 pixels by default) and the screen is its pixels in cells, 83.33 columns
for 1000 pixels. So:

- A container that shares out space snaps each edge with `snap_x` and `snap_y` (`text_metrics.h`).
- Where a position becomes a row, a column, a character or a buffer's cell it goes through `oui::whole(value)`
  (`geometry.h`), which takes a 2.9999999 that means 3 as 3. Never `static_cast<int>` a position.
- Text rows are a cell apart from wherever the widget starts; the row under the pointer is
  `whole(mouse.y - bounds().y)`.
- A widget as wide as its text asks `text_metrics().label_width(text)`, which is columns in a terminal and the
  proportional font's pixels in a window.
- `Length::pixels(p, n)` is `p` pixels in a window and `n` cells in a terminal, for gaps and strips that should be thin
  in a window.

Widgets never draw characters or pixels: they ask the `Painter` for elements (`draw_button`, `draw_window`,
`draw_tab`, `draw_label`...) and the terminal's painter or the graphical one decides how they look. [Rendering](oui_rendering.md)
has the painter, backends, fonts and how to write a `paint`; [themes](themes.md) the colours.

## Events, focus and commands

- An `Event` is a `std::variant`: `KeyEvent` (a named key, with `control`, `alt`, `shift`), `TextEvent` (a character
  typed: Ctrl+S is a `TextEvent` of `s` with `control`; letters come in lowercase with `shift`), `MouseEvent`,
  `ResizeEvent`, `FocusEvent`, `DropEvent` (files dropped on a window) and a few the application keeps.
- What a widget does not use goes up to its parent. The order in which a key is offered is long and matters
  (`Application::dispatch_event`): a popup first, then a widget's `claims_key`, then the application's shortcuts,
  then the focused widget and its parents. Shortcuts come before a widget's own use of a key.
- `set_key_handler` hears keys before a widget does, without a class of one's own; `set_pointer_handler` and
  `set_focus_handler` the same for the mouse and the focus.
- The mouse is captured from press to release; double and triple clicks are the application's (`clicks`).
- Commands: `application.commands()` is one registry that the shortcuts, the `MenuBar` and the command palette read.
  `register_command(id, title, callback)`, then `bind(id, {U's', true})` for Ctrl+S or `oui::KeyShortcut{oui::Key::f3}`.
  Binding an unregistered id throws. Shortcuts do not run while a dialog shows.

[Layout and events](oui_layout.md#how-events-reach-widgets) has the fifteen steps of the order.

## Dialogs and descriptions

`application.show_modal(std::move(window))` shows a window and returns its `DialogResult` when it closes; the call
waits, and the window and its widgets are destroyed as it returns, so their values are read in the callback that
closes it, or the window is kept with `show_modal_and_keep`. Ready-made: `message_box`, `prompt`, `file_dialog`,
`quick_pick`, `ask_color`, `show_command_palette`.

A window can be written as a description instead of code, a widget a line, indented:

```
Window "Rename" width=40
  Label "&Name:" buddy=name
  LineEdit id=name
  Row centered gap=tight
    Button "OK" id=ok default
    Button "Cancel" id=cancel
```

```cpp
auto ui{oui::build_ui(description)};
ui.get<oui::Button>("ok").on_clicked([&] { /* ui.get<oui::LineEdit>("name").text() */ });
static_cast<void>(application.show_modal(ui.take_window()));
```

[Descriptions](oui_descriptions.md) has every widget and property.

## The terminal and the window

A program builds one widget tree; only the backend it gives `Application` differs. `Application()` is the terminal;
for a window, built with `OLAGARRO_OUI_WINDOW=ON` and linked with `olagarro::OUIWindow`:

```cpp
#include <oui/graphics/window_backend.h>

oui::WindowOptions options;
options.title = "My program";
oui::Application application{oui::window_backend(options), oui::classic_theme()};
```

`window_backend` throws where no window can open, as with no display; a program that offers both picks by its command
line, as `examples/oui_term/main.cpp` does. `classic_style()`/`classic_theme()` and `dark_style()`/`dark_theme()`
(`graphics/graphical_painter.h`) are the window's looks.

## Testing

GoogleTest, under `ctest`. Most widget tests need no application: make the widget, `arrange` it, send it events with
`handle_event`, and look at what it holds. Those that need focus, popups, dialogs or the screen give `Application` a
`ScriptedBackend` (`scripted_backend({}, {40, 12})`): `push` an event, `run_frame`, then read the last frame's
`cell_at(x, y).glyph`. `image_backend` lays out by the pixel and draws into an image, for what happens between cells.

```sh
ctest --test-dir build --output-on-failure
build/olagarro_oui_tests --gtest_filter='OuiPopupListTests.*'
```

Olagarro's own suite should be run serially (`ctest -j1`) when a result is to be trusted: the clipboard tests share
the desktop's clipboard. [Testing](testing.md) has the rest.

## Scripts

`olagarro::OUIScript` embeds JavaScript: `ScriptEngine::define("app.greet", function)` gives scripts a call, `run`
runs a script with a time limit, and promises settle from C++. Every name a script can use has a comment marked `api`
beside the code that defines it, and OUI Code's repository builds its `oui.d.ts` and API reference from them, failing
a test when one is missing. oui_run, in OUI Code's repository, runs JavaScript files as whole programs with OUI's
widgets. [OUIScript](ouiscript.md); [scripting with oui_run](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md).

## Rules of the house

[CONTRIBUTING.md](../CONTRIBUTING.md) has them all; the ones a change most often meets:

- C++17. Warnings are errors, on every system the continuous integration builds (Linux x86-64 and ARM64, macOS ARM64
  and x86-64): code inside `#ifdef __linux__`, `__APPLE__` or `_WIN32` takes with it every helper only it uses, or the
  other systems fail on an unused function.
- `PascalCase` types, `snake_case` everything else, `m_` members, uniform initialisation, locals `const` by default,
  braces on every body on their own lines, `++ i` with a space, `Widget*` and `const Widget&` attached to the type.
- A `.cpp` outside the library's namespace says `using namespace olagarro;` and writes `oui::label`; never
  `using namespace olagarro::oui;`. Headers name everything in full.
- No C++ streams and no `std::regex` in OUI or the programs (a test enforces it): files through `oui::read_file` and
  C's files, regular expressions through `oui::Regex`.
- No global state: what an object needs it is given. Process-wide on purpose, and said so: the translation catalogs,
  the `TextMetrics` a graphical backend sets, the drag image.
- Every text the person sees goes through `oui::translate(context, text)`; every colour comes from the theme.
- Every bug fix has a test that fails without it.
- Format with `oui_run tools/format_cpp.js FILE...` (oui_run comes with OUI Code); without files it formats the whole
  tree.
- Code, comments and documents in English.

## Mistakes easy to make

- Casting a position to `int`, or comparing it with `==` to a whole number: positions fall between cells in a window.
  `whole()` it.
- A `TextEdit` or `DockLayout` in a `Column` before other widgets: they take all the height and the others vanish.
- Reading a dialog's fields after `show_modal` returns: they are gone by then.
- Holding a plain reference to a widget in a timer or posted callback that may run after it is removed: use
  `WidgetRef`.
- Expecting a key to reach a widget when the program binds it as a shortcut: shortcuts go first. A widget that must
  keep a key says so in `claims_key`.
- A test `MouseEvent` that gives only a pixel: give the cell's `x` and `y` too, or every quick press counts as a double
  click on the same cell.
- Opening a dialog from inside a callback that runs while a list or menu is still open: `post` it to the next frame.
- Building with the window module on but forgetting it changes the programs' names (`_gui`) and needs its own build
  tree.

## Where everything is

| Folder | What |
|--------|------|
| `oui/include/oui` | OUI's headers, one per widget or part; `oui/oui.h` brings the common ones; `graphics/` and `script/` for the other OUI targets |
| `oui/src` | One source per widget or part; `_posix` and `_win32` sources for what differs by system |
| `examples/` | `oui_term` (a terminal in a widget), `oui_desktop` (OUI's desktop), the terminal demo |
| `tests/` | A folder per library; `tests/oui` shows how every widget is tested |
| `cmake/` | `OlagarroEmbedFiles.cmake` |
| `tools/` | `build.sh`, the formatter and other scripts run with oui_run |
| `doc/` | These documents; [the index](README.md) says which to read for what |
