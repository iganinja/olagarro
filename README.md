# Olagarro

![Olagarro](doc/logo.png)

Olagarro is a small, portable C++17 utility library for games and applications.
Its documentation is in [doc](doc/README.md), in Markdown: OUI Code shows it, its links followed.

![OUI Code with its File menu open, in a terminal on the left and in its own window on the right](doc/images/oui_code_terminal_and_window.png)

OUI Code, a text editor made with OUI, the same program in a terminal and in a window of its own.

## Modules

- `ByteStream`: binary serialization and file I/O.
- `SignalSlot`: variadic signals and slots with safe disconnection.
- `Tasks`: dynamic task composition.
- `CTTasks`: static task composition for small binaries.
- `Concurrency`: standard-library-based task pool, cancelling and `concurrent_for`.
- `Effects`: the pieces picture and sound effects are made of: blurs, noise, tables, colour and blend modes, on the
  task pool; gain, filters, an equaliser, echo, bitcrush, mono and crackle, cut into blocks exactly by their tails.
- `OUI`: a small UI toolkit for terminals, desktop windows and the whole screen.
- `OUIScript`: JavaScript for OUI programs, run by QuickJS.

## Made with OUI

- [OUI Code](https://github.com/iganinja/oui_code): OUI Code, a text editor and file explorer; oui_run, which runs
  JavaScript files as programs; and oui_calc, a calculator. Its README tells how to install them, on Linux, macOS and
  Windows, or from an APT repository.
- [OUI Paint](https://github.com/iganinja/oui_paint): a raster image editor in the spirit of Paint.NET, with layers,
  indexed images with palettes, and plugins in JavaScript.

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/install
```

On Linux, `tools/build.sh` does the same; `tools/build.sh --gui` builds the programs able to open in a desktop window
as well, as `bin/oui_term_gui` and the rest.

## CMake consumer

```cmake
find_package(Olagarro CONFIG REQUIRED COMPONENTS Tasks SignalSlot)

target_link_libraries(my_app PRIVATE
    olagarro::Tasks
    olagarro::SignalSlot)
```

## ByteStream

```cpp
olagarro::ByteStream stream{};
stream.write(42).write(3.5f);

const int answer{stream.read<int>()};
const float value{stream.read<float>()};
```

## Tasks

```cpp
using namespace olagarro::tasks;

TaskExecutor executor{};
executor.add_task(serial(
    wait(1.0f),
    execute([] {})));
executor.update(1.0f);
```

## OUI

OUI builds text-mode applications that run in any terminal, locally or over SSH,
on Linux and Windows. It is event driven and uses no CPU while idle.

- Widgets: labels, buttons, check buttons, line and multi-line text editors,
  combo boxes, lists, tables, tree views, tabs, scroll views and a status bar.
- Layout: rows, columns, grids and a dock layout; windows can be moved and
  resized, with the mouse or from the keyboard after Ctrl+L, and remember where
  the user left them.
- Commands: one registry feeds keyboard shortcuts, a menu bar with separators,
  mnemonics and dynamic submenus, and a command palette.
- Dialogs: message boxes, prompts, and open, save and folder dialogs.
- Text files: load and save keep their encoding and line endings, and saving
  replaces the file atomically.
- Text editing: syntax colouring through a highlighter that works line by line,
  several cursors, bracket matching, search with regular expressions.
- Split views that nest, timers that cost nothing while they wait, and child
  processes whose output arrives on the UI thread as it comes.
- A folder watcher (`FolderWatcher`): what was added, removed, written to or
  renamed under a folder and every folder in it, asked when the program likes;
  the system tells it on Linux, and elsewhere, or when the system cannot, it
  compares what the folder holds. Files and folders go to the system's trash
  (`move_to_trash`), from where the person can put them back.
- A terminal in a widget (`TerminalView`): a shell, or any program, runs in a
  pseudo-terminal of its own, and the widget shows its screen as xterm would,
  colours, the full-screen programs' alternate screen and the history included.
- Every user-facing string and colour comes from options or the theme.
- Translations: texts go through `translate(context, text)` and come from JSON
  catalogs built into the program (`olagarro_embed_files` in CMake). OUI
  ships Spanish for its own texts, and a test checks that every catalog covers
  every text in the sources.

- Graphics (`olagarro::OUIGraphics`): the same program can show itself in
  pixels instead of a terminal. Widgets ask the painter for elements (a
  button, a window frame, a tab, a scrollbar) and the painter decides how they
  look: a terminal with characters, a graphical style with bevelled shapes,
  interface text in a proportional font and cells in a monospace one, both
  built in. The default look is inspired by Transport Tycoon; themes change at
  run time. Layout stays in cells, so nothing else changes.
- A desktop window (`olagarro::OUIWindow`, with `OLAGARRO_OUI_WINDOW=ON`):
  SDL3 opens it and brings the keyboard and the mouse; OUI draws everything
  inside it, its frame and title bar too, and the desktop still moves and
  resizes it as any other. Files dropped on it from the system reach the widget
  under them. It plays sound too (`system_audio_output`), stereo blocks whose
  frames played are a clock to show pictures by; `SilentAudioOutput`, in OUI
  itself, plays nothing and moves only when told, for tests. SDL3 is linked
  statically, from an installed copy or fetched and built with only what a
  window and sound need.
- The whole screen, with no desktop under it (`drm_backend`, Linux, with
  `OLAGARRO_OUI_DRM=ON`): straight through the kernel's mode setting and input
  devices, with nothing but Linux's headers, for a console or a system of its own.
- OUI's own desktop (`DesktopServer` and `desktop_backend`, Linux): one program
  has the screen and shows the others' windows, which draw in memory shared
  with it and hear their input from it through a local socket. They share its
  clipboard, and name their windows as the task bar shows them. In a terminal,
  as over SSH, the same desktop shows in characters, and its programs draw cells.

`examples/oui_term` is a terminal, the user's shell in a `TerminalView`, and `examples/oui_desktop` the desktop,
with a task bar that starts oui_term, and OUI Code and oui_calc when the system has them, in windows: in a window of
another desktop with `--window`, on the whole screen of a Linux console with `--drm`, or in characters with
`--terminal`, over SSH too.

```cpp
olagarro::oui::Application application;
auto window{olagarro::oui::window("Hello")};
window->add(olagarro::oui::label("Hello from OUI"));
application.set_window(std::move(window));
return application.exec();
```

## OUIScript

OUIScript runs JavaScript in [QuickJS-ng](https://github.com/quickjs-ng/quickjs), vendored in
`third_party/quickjs-ng` under its MIT licence. C++ defines what scripts can call; scripts hand back values
C++ reads and functions it calls later. Each run has a time limit, so a script that loops forever fails
instead of hanging the program.

```cpp
olagarro::oui::script::ScriptEngine engine;
engine.define("app.greet", [](const std::vector<olagarro::oui::script::Value>& arguments)
              {
                  std::cout << "Hello, " << arguments.at(0).to_string() << '\n';
                  return olagarro::oui::script::Value{};
              });
engine.run(R"(app.greet("terminal");)", "hello.js");
```

`RuleHighlighter` colours text by states of regular expression rules, as definitions of languages in
scripts describe them; OUI Code defines its languages and text commands this way.

oui_run, in [OUI Code's repository](https://github.com/iganinja/oui_code), runs JavaScript files as programs the
way node does, with OUI's widgets for scripts that want a window. [Scripting with
oui_run](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md) shows how things are done, and [the API
reference](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md) lists every name scripts can use, made
from the comments marked `api` in OUI's sources and in the programs'.

## Concurrency

`concurrent_for` spreads the elements of a range over the machine's cores and returns a future. Each call
gets an element and its index.

```cpp
// Every element processed in place.
std::vector<Image> images{load_images()};
auto resizing{olagarro::concurrency::concurrent_for(images.begin(), images.end(),
    [](std::size_t, Image& image) { image = resize(image, 640, 480); })};
// ...other work while the images resize...
resizing.get(); // waits, and rethrows the first exception a worker threw

// A value from each element, folded into one: the longest word of a book.
const std::vector<std::string> words{split_words(book)};
const auto longest{olagarro::concurrency::concurrent_for(words.begin(), words.end(),
    [](std::size_t, const std::string& word) { return word.size(); },
    olagarro::concurrency::reductor(std::size_t{0},
        [](std::size_t longest, std::size_t size) { return std::max(longest, size); }))
    .get()};
```

The functions run on several threads at once, so they must not touch shared state unguarded. The combine of
a `reductor` must give the same result however the values are grouped, as a sum or a maximum does; its initial
value counts once. `concurrent_for` requires random-access iterators. It runs on a `TaskPool`, the program's
(`TaskPool::shared()`) or one given first, and the thread waiting for the future does its part of the work.

A single operation goes to the pool with `submit`, which gives back a handle to cancel it; the work gets a
`CancelToken` to check. `LatestWins` runs requests where only the newest matters, cancelling the ones before:

```cpp
auto thumbnail{olagarro::concurrency::TaskPool::shared().submit(
    [&](const olagarro::concurrency::CancelToken&) { return make_thumbnail(path); })};
thumbnail.cancel(); // never runs if it has not started; if it has, it sees the token
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the project conventions.

## Licence

Olagarro is under the zlib licence, in [LICENSE](LICENSE). What it carries of others' keeps its own:

| What | Where | Licence |
|------|-------|---------|
| QuickJS-ng, the JavaScript engine | `third_party/quickjs-ng` | MIT, in its `LICENSE` |
| stb_truetype, which reads the interface font | `third_party/stb` | MIT or public domain, at the end of the file |
| Spleen, the cell font | `oui/fonts/spleen_*` | BSD 2-clause, in `oui/fonts/spleen_license.txt` |
| Jersey 10, the interface font | `oui/fonts/jersey10_regular.ttf` | SIL Open Font License 1.1, in `oui/fonts/jersey10_ofl.txt` |
| Noto Sans, the dark style's interface font | `oui/fonts/noto_sans_regular.ttf` | SIL Open Font License 1.1, in `oui/fonts/noto_sans_ofl.txt` |
| NanoSVG, which draws SVG pictures | `third_party/nanosvg` | zlib, in its `LICENSE.txt` |
| SDL3, fetched and linked in only with `OLAGARRO_OUI_WINDOW=ON` | not in the repository | zlib |
