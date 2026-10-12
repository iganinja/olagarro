# Building

How to configure and build Olagarro with CMake, which build trees there are and what each puts in `bin/`, how the
small portable Linux binaries are made and installed, how to build for Windows, and the tools that format the C++
and regenerate the script API.

- [Requirements](#requirements)
- [Configuring and building](#configuring-and-building)
- [Options](#options)
- [Targets](#targets)
- [Two build trees: terminal and window](#two-build-trees-terminal-and-window)
- [tools/build.sh](#toolsbuildsh)
- [Release binaries](#release-binaries)
- [Building for Windows](#building-for-windows)
- [Running the tests](#running-the-tests)
- [Embedding files](#embedding-files)
- [Installing the libraries](#installing-the-libraries)
- [Formatting the C++](#formatting-the-c)
- [Regenerating the script API](#regenerating-the-script-api)

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
as `tools/build_portable.sh --gui` installs them.

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

In a Debug build, OUI Code is compiled with `OLAGARRO_DEBUG_BUILD`, and writes every event it receives to
`oui_code_events.log` in the folder it runs in.

## Options

| Option | Default | What it does |
|--------|---------|--------------|
| `OLAGARRO_BUILD_TESTS` | `ON` | Builds the test suites and registers them with CTest. Needs GoogleTest. |
| `OLAGARRO_BUILD_EXAMPLES` | `ON` | Builds the programs: OUI Code, oui_run, oui_term, oui_calc, oui_desktop and the terminal demo. |
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
| `olagarro_oui_code` | `oui_code` | `oui_code_gui` |
| `olagarro_oui_run` | `oui_run` | `oui_run_gui` |
| `olagarro_oui_term` | `oui_term` | `oui_term_gui` |
| `olagarro_oui_calc` | `oui_calc` | `oui_calc_gui` |
| `olagarro_oui_desktop` (Linux only) | `oui_desktop` | `oui_desktop_gui` |
| `olagarro_oui_terminal_demo` | stays in the build tree | |

The desktop's task bar starts the programs by the names they have in its own build: CMake passes them to it as
`OLAGARRO_OUI_TERM_NAME`, `OLAGARRO_OUI_CALC_NAME` and `OLAGARRO_OUI_CODE_NAME`.

The test targets are `olagarro_tasks_tests`, `olagarro_cttasks_tests`, `olagarro_bytestream_tests`,
`olagarro_bytestream_unit_tests`, `olagarro_signalslot_tests`, `olagarro_concurrency_unit_tests`, `olagarro_oui_tests`
and `olagarro_oui_code_tests`, with `olagarro_signalslot_sanitized_tests` and `olagarro_oui_sanitized_tests` where the
sanitizers can run. To build one program only:

```sh
cmake --build build --target olagarro_oui_code
```

## Two build trees: terminal and window

The window module changes the programs, not only adds a library: it links SDL3 into them, defines the macros that
compile in their `--window` code (`OLAGARRO_OUI_CODE_WINDOW` and the like), and changes their names. So a terminal
build and a window build are two build trees, configured with `OLAGARRO_OUI_WINDOW` off and on:

| Tree | Configured with | Puts in `bin/` |
|------|-----------------|----------------|
| `build/` | `OLAGARRO_OUI_WINDOW=OFF`, Debug | `oui_code`, `oui_run`, `oui_term`, `oui_calc`, `oui_desktop` |
| `build-gui/` | `OLAGARRO_OUI_WINDOW=ON`, Debug | `oui_code_gui`, `oui_run_gui`, `oui_term_gui`, `oui_calc_gui`, `oui_desktop_gui` |
| `build-release/`, `build-gui-release/` | the same, Release | the same names as their Debug tree |

Both write to the same `bin/`, and the different names let the two sets sit side by side. OUI Code keeps its settings
in `oui_code.ini` beside its binary when it can write there (`SettingsLocation::portable`), under that name in both
builds, so `oui_code` and `oui_code_gui` share them. A window build still runs in the terminal when it
is started without `--window`, so it is the more complete of the two; the terminal build is the one that is shipped
for the terminal, being smaller and needing nothing of the display system. A change to OUI or to a program is only
in both binaries once both trees are built again:

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

## Release binaries

A release carries OUI Code, oui_run and oui_calc for Linux and Windows on x86-64, for Linux on 64-bit ARM
(aarch64), as a Raspberry Pi 3, 4 or 5 with a 64-bit system, and for macOS on Apple's processors (arm64) and Intel's
(x86_64), in two kinds: `terminal`, the
programs as the terminal build makes them, and `gui`, those that also open in a window. Both kinds name them the
same, `oui_code` and the rest: a system has one or the other, and the `_gui` names of a window build here are only
so the two sit side by side in `bin/`.
Two scripts make them, the same on a developer's machine as on GitHub's.

`tools/build_portable.sh` builds one system and kind in a container, with podman or docker, mounting the repository
read-only, and writes the programs to `dist/<system>-<processor>-<kind>/`:

```sh
tools/build_portable.sh                   # dist/linux-x86_64-terminal: oui_code, oui_run, oui_calc
tools/build_portable.sh --gui             # dist/linux-x86_64-gui: oui_code, oui_run, oui_calc
                                          # on an ARM machine: dist/linux-aarch64-terminal and -gui
tools/build_portable.sh --windows         # dist/windows-x86_64-terminal: oui_code.exe and the rest
tools/build_portable.sh --windows --gui   # dist/windows-x86_64-gui: oui_code.exe and the rest
```

All are MinSizeRel, with tests off, `OLAGARRO_STATIC_LINK=ON` and link-time optimisation, and stripped.

- **Linux terminal.** In Alpine 3.20, linked statically against musl, so they run on any Linux of their processor
  whatever its C and C++ runtime: x86-64 on an x86-64 machine, aarch64 on an ARM one. `oui_code` and `oui_calc` are
  built with clang, libc++ (with libc++abi, compiler-rt and LLVM's libunwind), `-Oz`, full link-time optimisation,
  lld and identical code folding (`-Wl,--icf=all`); `oui_run` with GCC, which makes it smaller. `oui_code` comes out
  at about 2.1 MB.
- **Linux gui.** In Ubuntu 22.04: SDL3 and the C++ runtime go inside, the C library and the display libraries come
  from the system, so they run on Ubuntu 22.04 (glibc 2.35) and anything newer. `oui_code` is about 3.6 MB.
  Ubuntu 22.04's GCC 11 warns of uninitialised values where there are none, so that one warning is not an error
  there.
- **Windows.** Cross-built in Ubuntu 22.04 with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), a fixed
  version checked against its SHA-256 sum, and linked statically: the `.exe` needs only the system's DLLs.
  `oui_code.exe` is about 2.5 MB, or 4.1 MB in the `gui` kind.
- **macOS.** Built on the Mac itself, as Apple's tools run in no container: `tools/build_portable.sh`, with or
  without `--gui`, run on a Mac makes `dist/macos-<processor>-<kind>`, arm64 on Apple's processors and x86_64 on
  Intel's. They run on macOS 11 and newer, linked against the system's libraries, which every Mac has, unused code
  dropped (`-Wl,-dead_strip`). They are not signed by Apple: a browser marks what it downloads, and macOS then asks
  before running it the first time (right-click, Open); `install.sh` downloads with curl, which marks nothing.

`tools/package_release.sh` then makes the release's files of whatever `dist/` holds, in `dist/release/`:

| File | What it is |
|------|------------|
| `olagarro-<system>-<processor>-<kind>.tar.gz`, `.zip` for Windows | the three programs of that kind, with `LICENSE` and `THIRD_PARTY_NOTICES.txt` |
| `<program>-<system>-<processor>-<kind>`, `.exe` for Windows | each program alone, as `oui_code-linux-x86_64-gui` or `oui_code-linux-aarch64-terminal` |
| `oui-pack-linux-<processor>-<kind>.deb` | the three Linux programs of that kind as a Debian package, `oui-pack` or `oui-pack-gui`, in `/usr/bin`, for `amd64` or `arm64` |
| `THIRD_PARTY_NOTICES.txt` | the licences of everything any of them carries, for whoever downloads one program alone |
| `SHA256SUMS` | the sum of each file |

`THIRD_PARTY_NOTICES.txt` holds the licences of what the programs carry: QuickJS-ng, stb_truetype, Spleen and
Jersey 10 in all, SDL3 in the `gui` ones, musl in the Linux terminal ones, and the mingw-w64 runtime and winpthreads
in the Windows ones. Those the source does not keep beside its code are in `licenses/`, whose README says where each
comes from.

The Debian packages take their version from the tag, `0.1.0` from `v0.1.0`, or from `git describe` between tags,
`0.1.0+3.ged0fda3`, which Debian orders after it; `OLAGARRO_VERSION` sets it. `sudo dpkg -i` installs one, and a newer
one upgrades it the same way. The `gui` one depends on `libc6 (>= 2.35)`, as its programs do. They carry the licences
as Debian keeps them, in `/usr/share/doc/<package>/copyright`, and a changelog saying where they were built from.
`lintian` finds them as they are meant to be: statically linked, and without position-independent code in the `gui`
programs, which make them run anywhere and small; it also notes they have no manual pages.

The same packages are in an APT repository at `https://iganinja.github.io/olagarro`, suite `stable`, component
`main`, so `apt upgrade` brings each release. `tools/apt_repository.sh <folder with the .deb files> <folder to make>`
makes it: the packages in `pool/main`, `Packages` for each architecture, and `Release` signed as `Release.gpg` and as
`InRelease` with the secret key in gpg's keyring, whose public part goes beside them as `olagarro.gpg` for systems to
trust. It holds one release only; older ones stay in GitHub's releases. `install.sh --apt` adds it to a system.

The names carry no version, so a release's files are always at the same address: GitHub's
`https://github.com/iganinja/olagarro/releases/latest/download/<file>` is the latest release's. The installers download
from there: `install.sh` on Linux and `install.ps1` on Windows show a menu of the programs and kinds, or take options to
choose without asking, check each file against `SHA256SUMS`, and install the programs, in `~/.local/bin` or in
`%LOCALAPPDATA%\Programs\olagarro`, which `install.ps1` adds to the user's `PATH`; or unpack a whole package into the
folder they run in. `OUI_INSTALL_DIR` and `OUI_DOWNLOAD_URL` change where the programs go and where they come from.

GitHub makes the releases with `.github/workflows/release.yml`. Pushing a version's tag, as `v0.1.0`, runs the tests
in a terminal build and in a `gui` one on x86-64 and ARM64 Linux, and on GitHub's Macs, Apple's and Intel's, four at
a time there and past every error of the build, then builds the ten kinds with `tools/build_portable.sh` in parallel,
each on a machine of its system and processor, then
makes the files with `tools/package_release.sh` and publishes them as the tag's release. Nothing is published when a
test fails. Then a tag's Debian packages become the APT repository, published on GitHub Pages, signed with the key in
the repository's `APT_SIGNING_KEY` secret. Started by hand from the Actions tab, it makes the same files and leaves them
as the run's artifact, publishing nothing and leaving the APT repository as it was.

That needs, once: GitHub Pages published by GitHub Actions (Settings, Pages, Source), tags allowed to deploy to the
`github-pages` environment (Settings, Environments, `github-pages`, a tag rule `v*`), and the signing key as the
secret, made with no passphrase, its armoured secret part as the value:

```sh
export GNUPGHOME="$(mktemp -d)"
gpg --batch --passphrase '' --quick-generate-key "olagarro APT repository" ed25519 sign never
gpg --armor --export-secret-keys > olagarro-apt-signing-key.asc
```

The file is the only way to sign new releases for systems that already trust the key: kept somewhere safe, it lets
the secret be made again; lost, a new key means each system adding the repository again.

## Building for Windows

The sources build for Windows: `oui/src` has a `_win32` source for each part that differs by system, the terminal
backend uses the Windows console and the pseudo-terminal uses ConPTY. `oui_desktop`, the DRM backend and OUI's desktop
are Linux only and are left out. With MSVC the warnings are `/W4 /WX`. With GCC targeting Windows (MinGW-w64) the
programs are always linked statically (`-static -static-libgcc -static-libstdc++`), whatever `OLAGARRO_STATIC_LINK`
says; with clang they are with `OLAGARRO_STATIC_LINK=ON`, and without it llvm-mingw's libc++ and libunwind come as DLLs
beside them. Linked statically, the `.exe` needs only the system's DLLs.

`tools/build_portable.sh --windows` makes the release's `.exe` files in a container, as
[release binaries](#release-binaries) tells. By hand, they are cross-compiled from Linux with a MinGW-w64 toolchain.
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
cmake --build build-windows --target olagarro_oui_code olagarro_oui_run
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
ctest --test-dir build -R OuiCode            # only the tests whose name matches
```

CTest runs the GoogleTest suites (each test discovered by `gtest_discover_tests`, named `Suite.Test`), and tests of
its own named `Olagarro.*`: smoke, stress and input tests of OUI Code run through its own command line
(`--smoke-test`, `--stress-test`, `--input-test`), checks of `--help`, the script API check, the sanitized suites, and
the end-to-end tests that drive OUI Code, oui_calc and oui_run in a pseudo-terminal (not on Windows, as they use shell
commands). The end-to-end tests take a few minutes. In a window build, the same tests run against the `_gui`
programs. [Testing](testing.md) describes each kind and how to write one.

## Embedding files

`cmake/OlagarroEmbedFiles.cmake` defines `olagarro_embed_files`, which builds files into a target: scripts, `.oui`
descriptions, translation catalogs, help texts, fonts.

```cmake
olagarro_embed_files(olagarro_oui_code FUNCTION embedded_oui_code_help
    FILES examples/oui_code/help/help.md examples/oui_code/help/help.es.md)
```

It generates a source defining `std::vector<std::string_view> embedded_oui_code_help()` (in `NAMESPACE`, when one is
given), which returns each file's contents in the order given, and `embedded_oui_code_help_names()`, each file's name
without its folders, by which `text_for_language` picks a file per language. The files go in compressed with gzip, which
CMake writes itself, as arrays of bytes rather than string literals, since compilers cap the length of a literal; they
come back whole the first time the function is called, through `olagarro::oui::inflate_gzip`, so the target must link
OUI. The time in each gzip header is zeroed, so the same file makes the same source at every configure and nothing
builds again. Each file is a configure dependency: editing it regenerates the source on the next build. Where the order
of the files matters, as for OUI Code's scripts, the C++ that reads them names them in the same order.

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
bin/oui_run tools/format_cpp.js                  # every C++ source in the repository
bin/oui_run tools/format_cpp.js oui/src/grid.cpp # only these
bin/oui_run tools/format_cpp.js --clang-format=/usr/bin/clang-format-18
```

The script runs clang-format with the repository's `.clang-format` on every `.h`, `.hpp`, `.cpp`, `.cc` and `.cxx`
file, in batches of forty so the command line stays within what every system takes. It leaves out `.git`,
`third_party` (other projects' code, kept as upstream wrote it), `dist`, and any folder whose name starts with `bin`
or `build`. Then it puts back the space the conventions want after a prefix increment or decrement, `++ index`, which
clang-format takes away; comments, strings, characters and raw string literals are left as they are. Run it before
submitting a change.

## Regenerating the script API

Every name a script can use has a comment marked `api` beside the code that defines it:

```cpp
// api(system): oui.readFile(path: string): string | null
// The file's text, or null when it cannot be read.
```

`tools/extract_api.js` reads them from every `.cpp`, `.h` and `.js` under `oui/` and `examples/` and writes
`oui/oui.d.ts`, the TypeScript declarations editors read, and [doc/oui_api.md](oui_api.md), the reference:

```sh
bin/oui_run tools/extract_api.js           # writes both files
bin/oui_run tools/extract_api.js --check   # writes nothing; fails when they are out of date
```

With `--check` it also fails when oui_run's own `oui` has a name no comment declares, or a comment declares one oui_run
does not have. CTest runs the check as `Olagarro.OuiScriptApi`, so an API change without the regenerated files fails
the tests. OUI Code builds `oui/oui.d.ts` into itself, for its completion of script names, so after regenerating it
OUI Code has to be built again. [OUIScript](ouiscript.md) explains the comment's format and the scripts each `<where>`
names.

Also in `tools/`, `generate_unicode_composition.js` writes `oui/src/unicode_composition.h`, what a Latin letter and a
combining diacritic make together as one character, from the JavaScript engine's own Unicode tables; it is run the
same way, when the table needs more letters or marks.
