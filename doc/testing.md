# Testing

How olagarro is tested: the C++ unit tests of each library, the checks CMake registers beside them, and the tests
that run OUI's programs in a pseudo terminal the way a person uses them. It ends with how to run one case and the
rules a change follows.

- [Running the tests](#running-the-tests)
- [What ctest runs](#what-ctest-runs)
- [Unit tests in C++](#unit-tests-in-c)
- [Widgets without a terminal](#widgets-without-a-terminal)
- [Programs in a pseudo terminal](#programs-in-a-pseudo-terminal)
- [oui_run's own tests](#oui_runs-own-tests)
- [Running one case](#running-one-case)
- [House rules](#house-rules)

## Running the tests

Tests are built unless `OLAGARRO_BUILD_TESTS` is `OFF` (or CTest's `BUILD_TESTING` is). They need GoogleTest and
GoogleMock (`libgtest-dev libgmock-dev` on Debian and Ubuntu). From the repository:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

`tools/build.sh` configures, builds and then runs `ctest --output-on-failure -j` with one job per core;
`--no-tests` skips that last step. The test programs stay in the build folder (`build/olagarro_tasks_tests`,
`build/olagarro_oui_tests`...); the runnable programs, `oui_code`, `oui_run` and the rest, land in `bin/`.

The build treats warnings as errors, tests included.

## What ctest runs

`CMakeLists.txt` registers two kinds of test. GoogleTest executables go through `gtest_discover_tests`, so every
`TEST` in them is a ctest test of its own, named `Suite.Case`. The rest are commands added with `add_test`, named
`Olagarro.Something`.

| ctest name | What it runs | What it checks |
|------------|--------------|----------------|
| `TaskExecutorTests.*`, `WaitTest.*`... | `olagarro_tasks_tests`, from `tests/tasks` | the [tasks library](tasks_library.md) |
| `ContainerTasksTests.*`... | `olagarro_cttasks_tests`, from `tests/cttasks` | CTTasks, in [concurrency.md](concurrency.md#cttasks) |
| `ByteStreamTests.*` | `olagarro_bytestream_unit_tests`, `tests/bytestream/bytestream_test.cpp` | [ByteStream](bytestream.md) |
| `Olagarro.ByteStream` | `olagarro_bytestream_tests`, `tests/bytestream/main.cpp` | ByteStream with strings, vectors and files |
| `Olagarro.PackageConsumer` | `cmake/TestPackageConsumer.cmake` | the package installed in a folder of its own, and `tests/package_consumer` built and run against it; not when cross-compiling |
| `SignalSlotTests.*` | `olagarro_signalslot_tests`, `tests/signalslot/main.cpp` | [signals](signalslot.md) |
| `Olagarro.SignalSlotSanitized` | the same tests built with AddressSanitizer and UBSan | each callback destroyed once, never used after, never leaked |
| `ConcurrencyTests.*` | `olagarro_concurrency_unit_tests` | [`concurrent_for`](concurrency.md) |
| `Oui...Tests.*` | `olagarro_oui_tests`, the files in `tests/oui` but `teardown_test.cpp` | OUI's widgets, layout, terminal, scripts, themes... |
| `Olagarro.OuiSanitized` | `tests/oui/teardown_test.cpp` with all of OUI's sources, sanitized | what goes wrong only as an application is taken down |
| `Olagarro.OuiLifecycleStress` | `olagarro_oui_tests` with one filter | `OuiApplicationTests.StressesWidgetLifetimeAndModalIsolation`, with a 30 s limit |
| OUI Code's suites | `olagarro_oui_code_tests`, `tests/oui_code/*.cpp` with OUI Code's sources | completion, the find bar, scripts, the UI preview, the session... |
| `Olagarro.OuiTerminalSmoke` | `examples/oui/terminal_demo.cpp --smoke-test` | the demo starts and draws |
| `Olagarro.OuiCodeSmoke` | `oui_code --smoke-test` | OUI Code starts, renders once, and exits |
| `Olagarro.OuiCodeStress` | `oui_code --stress-test` | OUI Code's dialogs and commands, played from a script of events, within 30 s |
| `Olagarro.OuiCodeInput` | `oui_code --input-test` | dead-key accents typed into a document come out as the expected text |
| `Olagarro.OuiCodeHelp`, `Olagarro.OuiDesktopHelp` | `--help` | the options print as `--help`, never as `-- help` |
| `Olagarro.OuiScriptApi` | `oui_run tools/extract_api.js --check` | `oui/oui.d.ts` and `doc/oui_api.md` match the `api` comments, and `oui` has no name without one |
| `Olagarro.OuiRunScripts` | `oui_run tests/oui_run/run_tests.js` | oui_run itself and the recipes |
| `Olagarro.OuiCodeEndToEnd` | `oui_run tests/oui_code/end_to_end.js` | OUI Code in a pseudo terminal, keys pressed |
| `Olagarro.OuiCalcEndToEnd` | `oui_run tests/oui_calc/end_to_end.js` | OUI Calc the same way |

Some are left out where they cannot run:

- The two sanitized tests are built only with GCC or Clang, not on Windows and not with `OLAGARRO_STATIC_LINK`,
  as the sanitizers' runtimes need dynamic linking. `Olagarro.SignalSlotSanitized` runs with
  `ASAN_OPTIONS=detect_leaks=1`: a callback a signal never destroys fails it. `Olagarro.OuiSanitized` turns leak
  detection off; it is there for memory used after it is freed.
- The three script-driven suites (`OuiRunScripts`, `OuiCodeEndToEnd`, `OuiCalcEndToEnd`) need `script(1)`,
  `mkfifo`, `mktemp` and a POSIX shell, so they are not registered on Windows. Their time limits are 300 s, 300 s
  and 120 s: each end-to-end case runs the program for a few seconds of keys.
- `tests/oui/pty_test.cpp`, `terminal_view_pty_test.cpp`, `keyboard_layout_test.cpp` and `desktop_test.cpp` join
  `olagarro_oui_tests` on Linux only; `pty_win32_test.cpp` on Windows only.
- OUI Code's suites and the program tests need `OLAGARRO_BUILD_EXAMPLES`, on by default.

A few tests read the repository rather than the program. `olagarro_oui_tests` and `olagarro_oui_code_tests` are
built with `OLAGARRO_SOURCE_DIR` pointing at it: `OuiLeanSourcesTests.NothingBringsStreamsOrStdRegex`
(`tests/oui/lean_sources_test.cpp`) fails when a source under `oui/` or `examples/` includes `<fstream>`,
`<sstream>`, `<iostream>`, `<regex>` or their kin, which bring about 200 KB of locales into a static build; the
translation tests check that every catalog covers every text the sources pass to `translate`; OUI Code's tests load
and run its built-in scripts.

## Unit tests in C++

Each library has a folder in `tests/` with its own executable. They use GoogleTest, and GoogleMock where a test
counts calls (`tests/tasks` and `tests/cttasks` link `GTest::gmock_main`). A test names what it checks as a
sentence, and its comments say why the case matters:

```cpp
TEST(SignalSlotTests, ACallbackConnectedWhileFiringIsCalledFromTheNextFire)
TEST(TaskExecutorTests, ATaskAddedDuringAnUpdateRunsOnTheNextOne)
TEST(OuiCheckListTests, AClickOnABoxOrSpaceTicksItAndSaysSo)
```

The tasks tests share two helpers: `tests/tasks/task_mock.h`, a GoogleMock task, and `tests/tasks/probe.h`, a task
that counts its starts and updates into a `ProbeLog` the test keeps, and finishes when the log says so. The log is
shared, so it can be read after the composite that owned the probe is gone:

```cpp
auto log{std::make_shared<Probes::ProbeLog>()};
executor.add_task(repeat(Probes::probe(log), 3));
executor.update(1.0f);
EXPECT_EQ(log->starts, 1);
```

`tests/bytestream/main.cpp` is older than the rest and is not a GoogleTest program: it prints `PASSED` or
`## FAILED ##` for each check, ends with 1 when any failed, and writes its file in the system's temporary folder.

## Widgets without a terminal

Most widget tests need no application at all. A widget is created, given a rectangle with `arrange`, sent events
with `handle_event`, and asked what it now holds, as in `tests/oui/check_list_test.cpp`:

```cpp
auto list{oui::check_list()};
list->add_item("bread", false);
list->arrange({0, 0, 30, 5});
EXPECT_TRUE(list->handle_event(oui::MouseEvent{1, 0, true}));
EXPECT_TRUE(list->is_checked(0));
```

When a test needs the whole application (focus, popups, dialogs, what is drawn), it gives `oui::Application` a
backend that is not a terminal. `oui::ScriptedBackend` (`oui/include/oui/scripted_backend.h`) is the one OUI
provides: it plays a list of events and keeps the last frame it was asked to present, or every frame with
`set_keep_frames(true)`. `push` adds an event; `Application::run_frame` handles what is queued and draws once, so a
test can send one event and look at the screen it leaves. `tests/oui/popup_list_test.cpp` does it this way:

```cpp
auto backend{oui::scripted_backend({}, {40, 12})};
scripted = backend.get();
application = std::make_unique<oui::Application>(std::move(backend));
// ... a window, its widgets, a popup ...

void send(const oui::Event& event)
{
    scripted->push(event);
    application->run_frame();
}
```

The last frame is a `CellBuffer`: `frame.cell_at(x, y).glyph` is the character there and the cell has its colours,
so a test reads a row of text or checks a colour. `set_one_event_per_frame(true)` hands out one event per drawn
frame, as a person reacts to the screen; `set_on_drained` runs a function once when the script runs out, which is
where a test reads what it wants and ends `exec()` or a modal loop. OUI Code's `--stress-test` and `--input-test`
play their events through it (`examples/oui_code/self_tests.cpp`).

Other backends serve other tests. `oui::RasterBackend` (`oui/include/oui/raster_backend.h`) draws cells into pixels
a test reads with `pixel_at`; `oui::image_backend` (`oui/include/oui/graphics/pixel_backend.h`) draws through the
graphical painter into an image, for `tests/oui/graphics_test.cpp`. Many test files also define a small backend of
their own, often called `MemoryBackend`, with a fixed size and a queue of events, when they need something the
scripted one does not do.

## Programs in a pseudo terminal

`tests/oui_code/end_to_end.js` and `tests/oui_calc/end_to_end.js` test OUI Code and OUI Calc as a person uses them:
the real program, in a real pseudo terminal, keys pressed one after another, and the screen read back. They are
JavaScript run by oui_run, and share `tests/lib/terminal_session.js`.

`place(program, files)` makes a temporary folder (`mktemp -d`), copies the program into it, and writes the files
given. The program keeps its settings beside itself, so a copy in a folder of its own never touches the settings,
session or recent files of anyone's own copy. `run(args, keys, {columns, rows})` on what `place` gave runs it there:

- `script -qec` gives the program a pseudo terminal, sized with `stty cols ... rows ...` (100 by 24 unless told),
  with `LANG=C` and `LANGUAGE=en` so the texts are the English ones.
- The keys go through a named pipe (`mkfifo`) the terminal reads, held open for reading and writing so the program
  never sees its input end.
- Each key is written once the program has answered the one before: until its output has grown and then been quiet
  for 200 ms, or 3 s at most for a key that changes nothing. The first key waits up to 15 s for the program to start.
  A machine with much else to do is waited for rather than pressed past. An empty string as a key waits 600 ms.
- After the last key the terminal is stopped under the program, and `screen()` replays what it wrote.

A key is the text a terminal sends for it: `"\r"` for Enter, `"\x1bOP"` for F1, `"\x1bt"` for Alt+T, `"\x13"` for
Ctrl+S, and SGR mouse reports such as `` `\x1b[<0;${column};${row}M` `` for a click. `session(program, args, files,
keys, size)` does `place`, one `run` and `remove` in one call; a case that runs the program twice, to see what it
keeps from one start to the next, uses `place` and calls `run` twice.

`screen()` understands what OUI writes: cursor moves (`CSI row;column H`) and colours (`CSI ... m`), skipping any
other sequence. What it returns has `row(n)`, the text of a row; `find(text, offset)`, the row, column and colours
of the first place a text shows, or `null`; `all()`; and `written`, the raw output, for what leaves no trace on the
screen. Colours are the SGR numbers, `"97;41"` for bright white on red:

```js
test("--diff opens the comparison alone, the removed line in red", async () => {
    const shown = await session(ouiCode, ["--diff", "left.txt", "right.txt"], files, []);
    expect("the tab", shown.find("left.txt vs right.txt") !== null, true);
    expect("the removed line", shown.find("gone")?.colours, "97;41");
});
```

`expect(what, actual, expected)` compares with `!==` and throws with both values. `run(cases, only)` runs every case,
or those whose name contains `only`, prints `ok` or `FAIL` and the reason for each, and ends with 1 when any failed.

## oui_run's own tests

`tests/oui_run/run_tests.js PATH_TO_OUI_RUN` tests oui_run with oui_run. Most cases write a script as `main.js` in a
temporary folder, run the oui_run given on it, and compare what it printed and its exit code (`runScript`, and
`runScriptWith` to give it input). Others:

- `inTerminal` runs a script with a window under `script(1)` in a 40 by 10 terminal and looks at the escape sequences
  it wrote, to check colours.
- `pressing` runs a script in a 60 by 14 pseudo terminal through `place` from `tests/lib/terminal_session.js`, with
  keys, as the end-to-end tests do.
- `recipe` runs a script from `examples/oui_run/recipes` in a folder filled with a fixture (Markdown notes, sources,
  a CSV, a JSON file, a binary file, photos), and returns what it printed, its code, and every file in the folder
  after, so a recipe that writes is checked by what it wrote. `calling` builds a driver that imports one of the
  `recipes/oui_code` scripts and calls it, as OUI Code would.

## Running one case

A GoogleTest case, through ctest or straight from its executable:

```sh
ctest --test-dir build -R 'SignalSlotTests' --output-on-failure
build/olagarro_tasks_tests --gtest_filter='TaskExecutorTests.*'
build/olagarro_oui_tests --gtest_filter='OuiPopupListTests.OpensUnderItsCellOverEverythingAndKeepsNoFocus'
```

A script suite takes the program to test and, second, part of a case's name; it runs from the repository:

```sh
bin/oui_run tests/oui_code/end_to_end.js bin/oui_code 'Calculator'
bin/oui_run tests/oui_calc/end_to_end.js bin/oui_calc
bin/oui_run tests/oui_run/run_tests.js bin/oui_run 'csv_summary'
```

The end-to-end tests run a copy of the program, never the one given, so a session of OUI Code open elsewhere is
left alone.

## House rules

From [CONTRIBUTING.md](../CONTRIBUTING.md) and the way the suites are written:

- Every bug fix includes a test that reproduces the bug: a test that fails without the fix and passes with it. Seeing
  it fail first is what shows it tests the bug; its comment says what used to go wrong, as
  `TaskExecutorTests.ATaskAddedDuringAnUpdateRunsOnTheNextOne` does.
- Tests follow the code conventions: C++ is formatted with `oui_run tools/format_cpp.js`, and warnings are errors.
- A name scripts can use has an `api` comment, or `Olagarro.OuiScriptApi` fails; after changing one, run
  `oui_run tools/extract_api.js` to remake `oui/oui.d.ts` and `doc/oui_api.md`.
- OUI and its programs use neither C++ streams nor `std::regex`; `OuiLeanSourcesTests` enforces it.
