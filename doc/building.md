# Building

How to configure and build Olagarro with CMake, which build trees there are and what each puts in `bin/`, how to
build for Windows, how to install the libraries, and the tools that format the C++.

- [Requirements](#requirements)
- [Configuring and building](#configuring-and-building)
- [Options](#options)
- [Targets](#targets)
- [Two build trees: terminal and window](#two-build-trees-terminal-and-window)
- [tools/build.sh](#toolsbuildsh)
- [Building for Windows](#building-for-windows)
- [Running the tests](#running-the-tests)
- [Embedding files](#embedding-files)
- [Installing the libraries](#installing-the-libraries)
- [Formatting the C++](#formatting-the-c)
- [The script API](#the-script-api)
- [Composed characters](#composed-characters)

## Requirements

CMake 3.20 or newer (3.28 for the C++20 module adapters), Ninja, a C++17 compiler, and GoogleTest with GoogleMock for
the tests. On Debian, Ubuntu or Mint:

```sh
sudo apt install cmake ninja-build g++ libgtest-dev libgmock-dev clang-format
```

The window module needs SDL3. When CMake finds no installed SDL3 it fetches SDL's source and builds it, and that
needs the development headers of the display system: X11 (`libx11-dev`, `libxext-dev`, `libxrandr-dev`,
`libxcursor-dev`, `libxi-dev`, `libxfixes-dev`, `libxss-dev`, `libxtst-dev`), `libxkbcommon-dev`, Wayland
(`libwayland-dev`, `libwayland-bin`, `wayland-protocols`), `libegl-dev`, `libgl-dev`, `libdrm-dev` and `libgbm-dev`,
as `tools/build_portable.sh --gui` in [OUI Code's repository](https://github.com/iganinja/oui_code) installs them.

The build treats every warning as an error (`-Wall -Wextra -Wpedantic -Werror`, or `/W4 /WX` with MSVC). Third-party
code (QuickJS-ng, stb_truetype, SDL3) is built without them.

## Configuring and building

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`CMAKE_EXPORT_COMPILE_COMMANDS` is on, so every build tree has a `compile_commands.json`; `.vscode/settings.json`
points VS Code at `build/compile_commands.json`.

## Options

| Option | Default | What it does |
|--------|---------|--------------|
| `OLAGARRO_BUILD_TESTS` | `ON` | Builds the test suites and registers them with CTest. Needs GoogleTest. |
| `OLAGARRO_BUILD_EXAMPLES` | `ON` | Builds the programs: oui_term, oui_desktop and the terminal demo. |
| `OLAGARRO_BUILD_MODULES` | `OFF` | Builds the experimental C++20 module adapters in `modules/`. Needs CMake 3.28. |
| `OLAGARRO_OUI_WINDOW` | `OFF` | Builds `olagarro::OUIWindow`, a desktop window on SDL3 linked statically, and lets the programs open in it with `--window`. The programs take a `_gui` name. |
| `OLAGARRO_OUI_DRM` | `OFF` | Lets the programs take the whole screen of a Linux console with `--drm`, through the kernel's mode setting, with no display server. |
| `OLAGARRO_STATIC_LINK` | `OFF` | Links the programs statically, so they run on Linux systems whose C and C++ runtime is older than the build machine's. |
| `OLAGARRO_RUNTIME_OUTPUT_DIRECTORY` | `<source>/bin` | Where the programs are written. |

`OLAGARRO_STATIC_LINK` does more than add `-static`. A static binary carries the whole runtime, so the option also
compiles with `-ffunction-sections -fdata-sections` and links with `-Wl,--gc-sections`, dropping every function nobody
calls; and, on Linux, with `-fno-pie` and `-no-pie`, since a program loaded at a fixed address needs no table of
addresses to fix up. A static libstdc++ alone would not be enough: it still asks for the build machine's glibc. With
the window module, though, only the C++ runtime goes inside (`-static-libgcc -static-libstdc++`): SDL3 loads the
system's display libraries when it starts, and those take the system's C library. Such a build runs on systems as new
as the one it was built on, or newer.

A build tree configured by hand for a special purpose, as one with sanitizers in `CMAKE_CXX_FLAGS` or one optimised for
size, should set `OLAGARRO_RUNTIME_OUTPUT_DIRECTORY` to a folder of its own, such as `<tree>/bin`, so its programs do
not replace the ones in `bin/`.

## Targets

The libraries are `olagarro_bytestream`, `olagarro_signalslot`, `olagarro_tasks`, `olagarro_cttasks`,
`olagarro_concurrency`, `olagarro_quickjs`, `olagarro_oui`, `olagarro_oui_graphics`, `olagarro_oui_script` and, with
the window module, `olagarro_oui_window`; [the overview](overview.md#dependencies) says what each links. The programs:

| Target | In `bin/` | With `OLAGARRO_OUI_WINDOW` |
|--------|-----------|----------------------------|
| `olagarro_oui_term` | `oui_term` | `oui_term_gui` |
| `olagarro_oui_desktop` (Linux only) | `oui_desktop` | `oui_desktop_gui` |
| `olagarro_oui_terminal_demo` | stays in the build tree | |

The desktop's task bar starts the programs it finds beside its own executable, by the names they have in a build of
its kind: CMake passes them to it as `OLAGARRO_OUI_TERM_NAME`, `OLAGARRO_OUI_CALC_NAME` and `OLAGARRO_OUI_CODE_NAME`,
`oui_calc` and `oui_code` being those [OUI Code's repository](https://github.com/iganinja/oui_code) builds.

The test targets are `olagarro_tasks_tests`, `olagarro_cttasks_tests`, `olagarro_bytestream_tests`,
`olagarro_bytestream_unit_tests`, `olagarro_signalslot_tests`, `olagarro_concurrency_unit_tests` and
`olagarro_oui_tests`, with `olagarro_signalslot_sanitized_tests` and `olagarro_oui_sanitized_tests` where the
sanitizers can run. To build one program only:

```sh
cmake --build build --target olagarro_oui_term
```

## Two build trees: terminal and window

The window module changes the programs, not only adds a library: it links SDL3 into them, defines the macros that
compile in their `--window` code (`OLAGARRO_OUI_TERM_WINDOW` and the like), and changes their names. So a terminal
build and a window build are two build trees, configured with `OLAGARRO_OUI_WINDOW` off and on:

| Tree | Configured with | Puts in `bin/` |
|------|-----------------|----------------|
| `build/` | `OLAGARRO_OUI_WINDOW=OFF`, Debug | `oui_term`, `oui_desktop` |
| `build-gui/` | `OLAGARRO_OUI_WINDOW=ON`, Debug | `oui_term_gui`, `oui_desktop_gui` |
| `build-release/`, `build-gui-release/` | the same, Release | the same names as their Debug tree |

Both write to the same `bin/`, and the different names let the two sets sit side by side. A window build still runs
in the terminal when it is started without `--window`, so it is the more complete of the two; the terminal build is
smaller and needs nothing of the display system. A change to OUI or to a program is only in both binaries once both
trees are built again:

```sh
cmake --build build && cmake --build build-gui
```

## tools/build.sh

`tools/build.sh` configures, builds and tests in one step, on Linux and other POSIX systems:

```sh
tools/build.sh [--release] [--gui] [--static] [--no-tests] [--clean]
```

| Option | Effect |
|--------|--------|
| (none) | Debug in `build/`. |
| `--release` | Release in `build-release/`. |
| `--gui` | `OLAGARRO_OUI_WINDOW=ON` in `build-gui/`, or `build-gui-release/` with `--release`. |
| `--static` | `OLAGARRO_STATIC_LINK=ON`. |
| `--no-tests` | Skips `ctest`. |
| `--clean` | Deletes the build tree first, for a cold build. |

It always uses Ninja, and runs `ctest --output-on-failure` with as many jobs as the machine has processors.

## Building for Windows

The sources build for Windows: `oui/src` has a `_win32` source for each part that differs by system, the terminal
backend uses the Windows console and the pseudo-terminal uses ConPTY. `oui_desktop`, the DRM backend and OUI's desktop
are Linux only and are left out. With MSVC the warnings are `/W4 /WX`. With GCC targeting Windows (MinGW-w64) the
programs are always linked statically (`-static -static-libgcc -static-libstdc++`), whatever `OLAGARRO_STATIC_LINK`
says; with clang they are with `OLAGARRO_STATIC_LINK=ON`, and without it llvm-mingw's libc++ and libunwind come as DLLs
beside them. Linked statically, the `.exe` needs only the system's DLLs.

They are cross-compiled from Linux with a MinGW-w64 toolchain; OUI Code's repository makes its released `.exe` files
this way, in a container.
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang, lld and libc++) gives the smallest binaries; GNU's
MinGW-w64 linker keeps every function's unwind data, so its `--gc-sections` drops almost nothing. A CMake toolchain
file for it looks like this:

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
# The toolchain's sysroot, and wherever a cross-built GoogleTest is installed.
set(CMAKE_FIND_ROOT_PATH /path/to/llvm-mingw/x86_64-w64-mingw32 /path/to/gtest-windows)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

```sh
cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=windows.cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DOLAGARRO_STATIC_LINK=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DOLAGARRO_RUNTIME_OUTPUT_DIRECTORY=$PWD/build-windows/bin -DOLAGARRO_BUILD_TESTS=OFF
cmake --build build-windows --target olagarro_oui_term
```

For the tests, GoogleTest has to be built with the same toolchain and installed where `CMAKE_FIND_ROOT_PATH` reaches,
and `CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST` keeps CMake from running the `.exe` test binaries on Linux
to list their tests while it builds. They then run on Windows, or under Wine, which needs `LANG=C.UTF-8` for
arguments outside ASCII to arrive whole, and Wine 11 or newer for ConPTY. With libc++, the C++ runtime's DLLs go
beside the test executables.

libc++ is stricter about includes than libstdc++: code that builds on Linux with GCC may miss an include there, as
`<cstdlib>` for `std::abs`.

## Running the tests

```sh
ctest --test-dir build --output-on-failure -j "$(nproc)"
ctest --test-dir build -R SignalSlot         # only the tests whose name matches
```

CTest runs the GoogleTest suites (each test discovered by `gtest_discover_tests`, named `Suite.Test`), and tests of
its own named `Olagarro.*`: the terminal demo's smoke test, the desktop's `--help`, the sanitized suites, a stress test
of OUI's application, and the installed package used by another project. [Testing](testing.md) describes each kind
and how to write one.

## Embedding files

`cmake/OlagarroEmbedFiles.cmake` defines `olagarro_embed_files`, which builds files into a target: scripts, `.oui`
descriptions, translation catalogs, help texts, fonts.

```cmake
olagarro_embed_files(olagarro_oui NAMESPACE olagarro::oui FUNCTION embedded_oui_calculator_help
    FILES oui/help/calculator.md oui/help/calculator.es.md)
```

It generates a source defining `std::vector<std::string_view> embedded_oui_calculator_help()` (in `NAMESPACE`, when one
is given), which returns each file's contents in the order given, and `embedded_oui_calculator_help_names()`, each
file's name without its folders, by which `text_for_language` picks a file per language. The files go in compressed with
gzip, which CMake writes itself, as arrays of bytes rather than string literals, since compilers cap the length of a
literal; they come back whole the first time the function is called, through `olagarro::oui::inflate_gzip`, so the
target must link OUI. The time in each gzip header is zeroed, so the same file makes the same source at every configure
and nothing builds again. Each file is a configure dependency: editing it regenerates the source on the next build.
Where the order of the files matters, the C++ that reads them names them in the same order.

## Installing the libraries

```sh
cmake --install build --prefix /path/to/install
```

installs the libraries, their headers under `include/olagarro`, and a CMake package, so a project can use them:

```cmake
find_package(Olagarro CONFIG REQUIRED COMPONENTS Tasks SignalSlot)
target_link_libraries(my_app PRIVATE olagarro::Tasks olagarro::SignalSlot)
```

The targets have the same names installed as in the repository, as `olagarro::SignalSlot` and `olagarro::OUIScript`
([the overview](overview.md#the-libraries) lists them). The window module is not installed. `tests/package_consumer`
is a small project that consumes the installed package; ctest's `Olagarro.PackageConsumer` installs the package in a
folder of its own, then builds and runs that project against it.

## Formatting the C++

```sh
oui_run tools/format_cpp.js                  # every C++ source in the repository
oui_run tools/format_cpp.js oui/src/grid.cpp # only these
oui_run tools/format_cpp.js --clang-format=/usr/bin/clang-format-18
```

oui_run is one of the programs of [OUI Code](https://github.com/iganinja/oui_code), whose README tells how to install
it.

The script runs clang-format with the repository's `.clang-format` on every `.h`, `.hpp`, `.cpp`, `.cc` and `.cxx`
file, in batches of forty so the command line stays within what every system takes. It leaves out `.git`,
`third_party` (other projects' code, kept as upstream wrote it), `dist`, and any folder whose name starts with `bin`
or `build`. Then it puts back the space the conventions want after a prefix increment or decrement, `++ index`, which
clang-format takes away; comments, strings, characters and raw string literals are left as they are. Run it before
submitting a change.

## The script API

Every name a script can use has a comment marked `api` beside the code that defines it, as OUI's calls do in
`oui/src/script/system_api.cpp`:

```cpp
// api(system): oui.readFile(path: string): string | null
// The file's text, or null when it cannot be read.
```

OUI Code's repository reads them, from OUI's sources and from its programs', and makes `oui.d.ts` and [the API
reference](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md) of them; [oui_run's
host](https://github.com/iganinja/oui_code/blob/master/doc/oui_run.md#the-api-comments-and-the-generated-reference)
explains the comment's format. A change here to a name scripts can use reaches them once that repository's
submodule points at it and the files are made again there.

## Composed characters

`tools/generate_unicode_composition.js` writes `oui/src/unicode_composition.h`, what a Latin letter and a
combining diacritic make together as one character, from the JavaScript engine's own Unicode tables. It is run with
oui_run from the repository, `oui_run tools/generate_unicode_composition.js`, when the table needs more letters or
marks.
