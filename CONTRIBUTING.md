# Contributing to Olagarro

## Baseline

All Olagarro code is built as C++17. Configure from the repository root:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

On Linux, `tools/build.sh` does the three steps (`--release`, `--no-tests`, and
`--clean` for a cold build). `--static` links the programs fully statically: they
then run on Linux systems whose C and C++ runtime is older than the build
machine's. The runnable examples, `oui_term` among them, land in `bin/`.

Linux prerequisites (Debian, Ubuntu, Mint):

```sh
sudo apt install cmake ninja-build g++ libgtest-dev libgmock-dev clang-format
```

OUI's clipboard uses `wl-copy`/`wl-paste`, `xclip`, or `xsel` when present to
share the desktop's; without them it copies through the terminal (OSC 52).

The build treats warnings as errors. Fix every warning instead of suppressing it.

Code for one system, inside `#ifdef __linux__`, `__APPLE__` or `_WIN32`, takes with it into the same block every helper
only it uses: built elsewhere, an unused function is a warning, so an error, that the system you build on never shows.
The continuous integration builds on macOS too; a change is not done until it passes there.

## Conventions

- Types, classes, structs, enums, and aliases use `PascalCase`.
- Namespaces, functions, methods, parameters, and variables use `snake_case`.
- A `.cpp` outside a library's own namespace, a program's or a test's, says `using namespace olagarro;` after its
  includes and names what it uses as `oui::label`, `signal_slot::Connection`: short, and still telling where each
  comes from. Not `using namespace olagarro::oui;`, whose names would meet a program's own, as a variable called
  `window` or `label`. A header names everything in full, as a `using` in it reaches all that include it.
- Class members use the `m_` prefix followed by `snake_case`.
- Use uniform initialization and default local variables to `const`.
- Parameters passed by value are not `const`: it is not part of the signature and adds only noise. `const`
  stays where it means something to the caller, on what references and pointers refer to: `const Widget&`,
  `const char*`.
- Attach pointer and reference declarators to the type: `Widget*`, `const Widget&`, and `auto&`.
- Write literal suffixes in lowercase: `1.0f`, `5u`, `1024ull`.
- Every `if`, `else`, `for`, `while`, `do`, and `switch` body uses braces. Put
  the opening brace on the following line and the closing brace on its own line.
- Use prefix increments and decrements when their value is not needed. Leave a
  space after the operator: `++ iterator`, never `iterator++`.
- Format C++ with `oui_run tools/format_cpp.js`, from the repository, before submitting a change, or with
  `oui_run tools/format_cpp.js FILE...` for some files. It applies the repository `.clang-format` configuration
  and restores the required spaces in prefix increments and decrements. oui_run comes with
  [OUI Code](https://github.com/iganinja/oui_code).
- New code, identifiers, comments, and technical proposals are written in English.
- Every name scripts can use, `oui.readFile` or `console.log`, has a comment marked `api` beside the code that
  defines it, as in `oui/src/script/system_api.cpp`. [OUI Code's repository](https://github.com/iganinja/oui_code)
  makes its `oui.d.ts`, for editors, and its `doc/oui_api.md`, for readers, from the comments in OUI's sources and in
  its programs', and a test there fails when either is out of date or oui_run's `oui` has a name without one.
- Public APIs make ownership explicit.
- OUI and the programs here do not use C++'s streams (`<fstream>`, `<sstream>`, `<iostream>`...) or `std::regex`: in a
  static build they bring all of libc++'s or libstdc++'s locales, about 200 KB. Files go through
  `olagarro::oui::read_file` and C's files, regular expressions through `olagarro::oui::Regex`; a test checks it.

## Where things are

- `oui/` is the library: its headers in `oui/include/oui`, one source for each widget or part in `oui/src`. A big
  class may take several sources, named after it: `text_edit.cpp` keeps TextEdit's state, and
  `text_edit_input.cpp`, `text_edit_lines.cpp`, `text_edit_search.cpp` and `text_edit_view.cpp` the rest.
- `examples/oui_term` and `examples/oui_desktop` are programs built on OUI, each with a short `main.cpp` and its
  parts beside it; `examples/oui` is the small terminal demo the tests run.

## Design

- Avoid global state and implicit dependencies.
- Every bug fix includes a test that reproduces it.
- `CTTasks` keeps static composition without virtual dispatch or heap allocation.
- `Tasks` destroys deep dynamic trees iteratively.
