# Overview

Olagarro is a set of C++17 libraries and a few programs built on them. This document is where to start: what each
library and program is for, how they depend on each other, where things are in the repository, and the ideas that run
through all of it. Each part has a document of its own for the detail.

- [The libraries](#the-libraries)
- [The programs](#the-programs)
- [Dependencies](#dependencies)
- [Repository layout](#repository-layout)
- [Ideas that run through everything](#ideas-that-run-through-everything)
- [Where to read next](#where-to-read-next)

## The libraries

Each library is a CMake target with a name in the `olagarro::` namespace, the same inside the repository and in an
installed package, where `find_package(Olagarro)` gives it to a consumer by its component, the name after `::`.

| Target | What it is |
|--------|------------|
| `olagarro::ByteStream` | Binary serialisation into a byte buffer, and file I/O. [ByteStream](bytestream.md) |
| `olagarro::SignalSlot` | Header-only variadic signals and slots, with connections that can be dropped safely while a signal is being emitted. [Signals and slots](signalslot.md) |
| `olagarro::Tasks` | Tasks composed at run time: serial, parallel, select, repeat, wait, trigger and others, driven by a `TaskExecutor` one update at a time. [Tasks](tasks_library.md) |
| `olagarro::CTTasks` | The same idea composed at compile time, with templates: no virtual dispatch and no heap allocation, for small binaries. [Tasks](tasks_library.md) |
| `olagarro::Concurrency` | Header-only: a task pool whose work can be cancelled, "latest wins" requests, and `concurrent_for`, which spreads a range over the pool and returns a future. [Concurrency](concurrency.md) |
| `olagarro::Effects` | The pieces picture effects are made of: blurs, noise, tables, colour changes, vignettes and blend modes, on linear premultiplied floats, their rows shared over a task pool; and those sound effects are made of: gain, filters, an equaliser, echo, bitcrush, mono and crackle, on stereo floats, each with the tail that lets a block be cut anywhere. [Effects](effects.md) |
| `olagarro::OUI` | The UI toolkit: widgets, layout, commands, dialogs, text editing, processes, pseudo-terminals, translations, settings, and the terminal backend. |
| `olagarro::OUIGraphics` | OUI in pixels: a canvas, the fonts built into the program, the graphical painter, the full-screen DRM backend and the pixel side of OUI's desktop. |
| `olagarro::OUIWindow` | A desktop window and the system's sound through SDL3, linked statically. Only with `OLAGARRO_OUI_WINDOW=ON`, and not installed: SDL3 is fetched and built into it, and is no package of its own to depend on. |
| `olagarro::OUIScript` | OUIScript: JavaScript run by QuickJS-ng, the API scripts see of the system, and the highlighter that colours text by rules a script defines. [OUIScript](ouiscript.md) |
| `olagarro::QuickJS` | QuickJS-ng, vendored in `third_party/quickjs-ng` and built as C11 without Olagarro's warnings. |

OUI is the largest part by far. Its own documents cover [layout](oui_layout.md), [user interface
descriptions](oui_descriptions.md) (`.oui` files), [rendering](oui_rendering.md), [themes](themes.md) and
[translations](translations.md).

`modules/` holds optional C++20 module adapters (`import olagarro;`) over the C++17 headers, built only with
`OLAGARRO_BUILD_MODULES=ON`; `modules/README.md` describes them.

## The programs

The programs here live in `examples/`. Each is a short `main.cpp` that picks where the program shows, with the rest of
it in sources beside it or in OUI itself. They land in `bin/` (see [Building](building.md)).

| Program | Source | What it is |
|---------|--------|------------|
| `oui_term` | `examples/oui_term` | A terminal: the user's shell, or the program given, in OUI's `TerminalView`. |
| `oui_desktop` | `examples/oui_desktop` | OUI's own desktop, Linux only: a task bar that starts the programs beside it and shows their windows. |
| `olagarro_oui_terminal_demo` | `examples/oui/terminal_demo.cpp` | A small OUI program that the tests run as a smoke test. |

Built with the window module, `oui_term` and `oui_desktop` take a `_gui` name (`oui_term_gui`, `oui_desktop_gui`), to
sit beside the terminal ones, and accept `--window`. Built with `OLAGARRO_OUI_DRM=ON`, both accept `--drm`, the whole
screen of a Linux console. On Linux, `oui_term` accepts `--desktop`, to show in a window on OUI's desktop, and
`oui_desktop` runs in characters with `--terminal`. The desktop's task bar starts `oui_term`, and OUI Code and oui_calc
when they are beside it, by the names they have in a build of the same kind.

Larger programs made with OUI have repositories of their own, which carry olagarro as a git submodule:
[OUI Code](https://github.com/iganinja/oui_code) holds OUI Code, the text editor, oui_run, which runs JavaScript files
as programs, and oui_calc, a calculator; [OUI Paint](https://github.com/iganinja/oui_paint) is an image editor.

## Dependencies

What each target links, as `CMakeLists.txt` says. "Private" means a consumer of the target does not see it.

| Target | Depends on |
|--------|------------|
| ByteStream | the C++ standard library only |
| signal_slot | the C++ standard library only |
| Tasks | nothing; `tasks/waitsignal.h` alone includes the signals, and whoever includes it links `signal_slot` |
| CTTasks | nothing |
| concurrency | `Threads::Threads` |
| Effects | concurrency |
| quickjs | `Threads::Threads`, and `m` outside Windows |
| OUI | `signal_slot`, `Threads::Threads`; `quickjs` privately, for its regular expressions; on Windows `user32`, `dbghelp`, `shell32`, `ole32`, `uuid` |
| oui_graphics | OUI; `third_party/stb/stb_truetype.h`, compiled into it |
| oui_window | oui_graphics; SDL3 (static) privately |
| oui_script | OUI; `quickjs` privately |

And the programs:

| Program | Always | With `OLAGARRO_OUI_WINDOW` | With `OLAGARRO_OUI_DRM` |
|---------|--------|----------------------------|-------------------------|
| `oui_term` | OUI | oui_window | oui_graphics |
| `oui_desktop` | OUI | oui_window, oui_graphics | oui_graphics |

On Linux, with either option on, `oui_term` also shows in pixels on OUI's desktop, and so links oui_graphics.

The small libraries (ByteStream, SignalSlot, Tasks, CTTasks, Concurrency, Effects) stand alone, but for Effects,
which runs on Concurrency's pool: OUI uses only SignalSlot of them, for its widgets' signals, such as
`Button::clicked`.

## Repository layout

| Folder | What it holds |
|--------|---------------|
| `bytestream/`, `signalslot/`, `concurrency/`, `effects/` | The small libraries, a header each (Effects two, for pictures and sounds), and for ByteStream and Effects their sources. |
| `tasks/`, `cttasks/` | Tasks and CTTasks: headers in `include/tasks` and `include/cttasks`, sources in `src/`. |
| `oui/include/oui` | OUI's public headers, one for each widget or part; `oui/oui.h` includes the common ones. `graphics/` and `script/` hold the headers of oui_graphics, oui_window and oui_script. |
| `oui/src` | One source for each widget or part, with `_posix` and `_win32` sources for what differs by system (terminal, processes, pseudo-terminals, files, clipboard, settings, crash reports, language); `graphics/` and `script/` as above. A big class takes several sources named after it, as `text_edit.cpp`, `text_edit_input.cpp`, `text_edit_view.cpp`. |
| `oui/fonts` | The fonts built into oui_graphics: Spleen bitmap fonts for cells, Jersey 10 and Noto Sans for interface text, with their licences. |
| `oui/translations`, `oui/help` | OUI's own Spanish catalog, and the calculator's help in English and Spanish, built into the library. |
| `examples/` | The programs, a folder each, and the terminal demo in `examples/oui`. |
| `tests/` | A folder for each library: GoogleTest suites in C++. `tests/package_consumer` is a small project that consumes the installed package. |
| `cmake/` | `OlagarroEmbedFiles.cmake`, which builds files into a program, and the package configuration template. |
| `tools/` | `build.sh`, and the scripts, run with oui_run, that format the C++ and generate OUI's table of composed characters. |
| `third_party/` | QuickJS-ng, stb_truetype and NanoSVG, as upstream wrote them. |
| `modules/` | The C++20 module adapters. |
| `doc/` | This documentation. |
| `licenses/` | The licences of what programs built on OUI carry that the source does not keep beside its code: stb_truetype's, SDL3's, and those of the C runtimes of static builds. |
| `.github/workflows/` | `tests.yml`, which tests every push to master on Linux and macOS. |

Build output stays out of the repository's history: `build/` and `build-*/` are build trees, and `bin/` holds the
programs.

## Ideas that run through everything

### Cells are the unit of layout

Every widget is laid out in cells, columns and rows, held as `double`s. How fine a position may be is the backend's
step (`TextMetrics::column_step()` and `row_step()`): a whole cell in a terminal, which lays out as it always did, and
a pixel in a graphical backend, which measures a cell by its cell font (`PixelBackend::cell_size`) and gives the
application every pixel it has, in cells. Containers snap the edges they compute to that step, and a position becomes a
row, a column or a character through `oui::whole`. So the same code lays out whole cells in a terminal and pixels in a
window: a widget as wide as its text is as wide as the text's pixels there, and gaps given as `Length::pixels` are
thin. Interface text shows in a proportional font, frames as bevelled shapes; where a widget places something by the
width of that text, as a text field's cursor, it measures through `TextMetrics`, which counts columns in a terminal and
pixels in a graphical backend. OUI's desktop keeps the same rule between programs:
each window is a whole number of cells of the same size, so what each program draws lines up with the desktop's cells.
[Layout](oui_layout.md) and [rendering](oui_rendering.md) go into this.

### One code path for the terminal and the window

A program builds one widget tree and one `Application`; only the `Backend` it hands the application differs. oui_term's
`main.cpp` picks it from the command line:

```cpp
    if(in_window)
    {
#ifdef OLAGARRO_OUI_TERM_WINDOW
        oui::WindowOptions window_options;
        window_options.title = "OUI Term";
        // ...
        backend = oui::window_backend(window_options);
        theme = oui::classic_theme();
#else
        std::fputs("oui_term: this build cannot open windows\n", stderr);
        return 2;
#endif
    }
    // ...
    else
    {
        backend = oui::terminal_backend();
    }
    oui::Application application{std::move(backend)};
```

The backends are `AnsiTerminalBackend` (a terminal, Linux or Windows), `DesktopWindow` (SDL3), `DrmBackend` (the
whole screen), `CellDesktopClient` and `PixelDesktopClient` (a window on OUI's desktop), and, for tests,
`ScriptedBackend`, `ImageBackend` and `RasterBackend`. Widgets never draw characters or pixels themselves: they ask a
`Painter` for elements, as `draw_window`, `draw_button` or `draw_tab`, saying what the element shows and in which state.
The terminal's painter, `CellBufferPainter`, draws them with characters into a `CellBuffer`; `GraphicalPainter` draws
them in pixels in a `GraphicalStyle`. So a widget written once shows in both, and the window build of a program still
runs in a terminal when it is started without `--window`.

### Scripts on top of C++

C++ provides the machinery; much of what a program does is written in JavaScript or in descriptions on top of it.
OUI Code's languages (how each is coloured, indented and commented) and its text commands are scripts built into the
program; its dialogs and panels are `.oui` user interface descriptions, which `build_ui` turns into widgets while C++
finds them by id and wires them. oui_run makes JavaScript a way to write whole programs, and the repository's own tools
(`tools/format_cpp.js`, `tools/generate_unicode_composition.js`) are scripts it runs. Every name a script can use has
a comment marked `api` beside the C++ or JavaScript that defines it, from which OUI Code's repository generates
`oui.d.ts` and [the API reference](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md). Files built into
a program (scripts, catalogs, help, fonts, `.oui` descriptions) go in compressed through `olagarro_embed_files`, so
each program is a single file. See [OUIScript](ouiscript.md), [user interface descriptions](oui_descriptions.md) and
[scripting with oui_run](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md).

### No global state

Objects receive what they need from whoever creates them. An `Application` owns its backend, theme, command registry,
clipboard and timers; a widget knows only its parent, and is handed the painter and the theme when it paints
(`Widget::paint(Painter&, const Theme&)`) and the event when one comes. Code that needs the application is given it.
So the tests make as many applications as they like in one process, each on a backend of its own. A few things are
process-wide on purpose, and say so in their headers: the installed translation catalogs (`install_translation`,
meant to be called once before the interface is built), the `TextMetrics` a graphical backend sets while it lives,
and the drag image under the pointer.

### Small, self-contained binaries

The programs are meant to be copied to a machine and run, with nothing installed beside them. That shapes the code:
OUI uses neither C++ streams nor `std::regex`, which in a static build bring in all of the C++ library's locales
(about 200 KB); files go through `olagarro::oui::read_file` and C's files, regular expressions through
`olagarro::oui::Regex`, on QuickJS's engine, and a test (`tests/oui/lean_sources_test.cpp`) enforces it. SDL3 is
linked statically, and `OLAGARRO_STATIC_LINK` drops every function nobody calls. [Building](building.md) tells how.

### Tests

Every bug fix comes with a test that reproduces it. The libraries have GoogleTest suites; OUI's widgets are tested
without a terminal through `ScriptedBackend`, which replays events and keeps the frames the application drew, and in
pixels through `ImageBackend`; the signals and OUI itself run again under AddressSanitizer and
UndefinedBehaviorSanitizer. Everything runs under `ctest`. [Testing](testing.md) explains it.

## Where to read next

- [Building](building.md): configuring, the build trees, static and Windows binaries, tests and tools.
- [Layout](oui_layout.md), [descriptions](oui_descriptions.md), [rendering](oui_rendering.md), [themes](themes.md),
  [translations](translations.md): OUI.
- [OUIScript](ouiscript.md): scripts, from C++.
- [Testing](testing.md).
- [Signals and slots](signalslot.md), [ByteStream](bytestream.md), [Tasks](tasks_library.md),
  [Concurrency](concurrency.md): the small libraries.
- `CONTRIBUTING.md` in the repository root: the code conventions.
- [OUI Code's documents](https://github.com/iganinja/oui_code/blob/master/doc/README.md): the editor, oui_run and
  scripting with it.
