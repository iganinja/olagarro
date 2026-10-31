# Testing

How olagarro is tested: the C++ unit tests of each library, the checks CMake registers beside them, and OUI's widgets
tested without a terminal. It ends with how to run one case and the rules a change follows. The programs made with
OUI in [OUI Code's repository](https://github.com/iganinja/oui_code) are also tested there as a person uses them, in a
pseudo terminal, as [its testing document](https://github.com/iganinja/oui_code/blob/master/doc/testing.md) tells.

- [Running the tests](#running-the-tests)
- [What ctest runs](#what-ctest-runs)
- [Unit tests in C++](#unit-tests-in-c)
- [Widgets without a terminal](#widgets-without-a-terminal)
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
`build/olagarro_oui_tests`...); the runnable programs, `oui_term` and `oui_desktop`, land in `bin/`.

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
| `ConcurrencyTests.*` | `olagarro_concurrency_unit_tests`, `tests/concurrency/concurrency_test.cpp` | [`concurrent_for`](concurrency.md), on the pool and with the caller helping |
| `TaskPoolTests.*`, `CancelTokenTests.*`, `LatestWinsTests.*` | `olagarro_concurrency_unit_tests`, `tests/concurrency/pool_test.cpp` | [the task pool, cancelling and latest wins](concurrency.md#the-task-pool) |
| `Oui...Tests.*` | `olagarro_oui_tests`, the files in `tests/oui` but `teardown_test.cpp` | OUI's widgets, layout, terminal, scripts, themes, suggestions... |
| `Olagarro.OuiSanitized` | `tests/oui/teardown_test.cpp` with all of OUI's sources, sanitized | what goes wrong only as an application is taken down |
| `Olagarro.OuiLifecycleStress` | `olagarro_oui_tests` with one filter | `OuiApplicationTests.StressesWidgetLifetimeAndModalIsolation`, with a 30 s limit |
| `Olagarro.OuiTerminalSmoke` | `examples/oui/terminal_demo.cpp --smoke-test` | the demo starts and draws |
| `Olagarro.OuiDesktopHelp` | `oui_desktop --help` | the options print as `--help`, never as `-- help`; Linux only |

Some are left out where they cannot run:

- The two sanitized tests are built only with GCC or Clang, not on Windows and not with `OLAGARRO_STATIC_LINK`,
  as the sanitizers' runtimes need dynamic linking. `Olagarro.SignalSlotSanitized` runs with
  `ASAN_OPTIONS=detect_leaks=1`: a callback a signal never destroys fails it. `Olagarro.OuiSanitized` turns leak
  detection off; it is there for memory used after it is freed.
- `tests/oui/pty_test.cpp`, `terminal_view_pty_test.cpp`, `keyboard_layout_test.cpp` and `desktop_test.cpp` join
  `olagarro_oui_tests` on Linux only; `pty_win32_test.cpp` on Windows only.
- The program tests need `OLAGARRO_BUILD_EXAMPLES`, on by default.

A few tests read the repository rather than the program. `olagarro_oui_tests` is built with `OLAGARRO_SOURCE_DIR`
pointing at it: `OuiLeanSourcesTests.NothingBringsStreamsOrStdRegex` (`tests/oui/lean_sources_test.cpp`) fails when a
source under `oui/` or `examples/` includes `<fstream>`, `<sstream>`, `<iostream>`, `<regex>` or their kin, which bring
about 200 KB of locales into a static build; the translation tests check that OUI's catalog covers every text its
sources pass to `translate`; `OuiInflateTests` compares files built into the tests with the same files in the
repository. The completion and suggester tests read `tests/oui/sample_api.d.ts`, the declarations of a script API kept
there as test data.

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
play their events through it.

Other backends serve other tests. `oui::RasterBackend` (`oui/include/oui/raster_backend.h`) draws cells into pixels
a test reads with `pixel_at`; `oui::image_backend` (`oui/include/oui/graphics/pixel_backend.h`) draws through the
graphical painter into an image, for `tests/oui/graphics_test.cpp`; there a layout is laid out by the pixel, as in a
window, so a test of where things fall between cells uses it, and the same layout on a scripted backend shows the
whole cells a terminal keeps. A `MouseEvent` a test makes for it may give `x` and `y` between cells, as the window's
pointer does (`ImageBackend::cell_at(pixel)`), or the whole cell and `cell_offset`. Many test files also define a small backend of
their own, often called `MemoryBackend`, with a fixed size and a queue of events, when they need something the
scripted one does not do.

## Running one case

A GoogleTest case, through ctest or straight from its executable:

```sh
ctest --test-dir build -R 'SignalSlotTests' --output-on-failure
build/olagarro_tasks_tests --gtest_filter='TaskExecutorTests.*'
build/olagarro_oui_tests --gtest_filter='OuiPopupListTests.OpensUnderItsCellOverEverythingAndKeepsNoFocus'
```

## House rules

From [CONTRIBUTING.md](../CONTRIBUTING.md) and the way the suites are written:

- Every bug fix includes a test that reproduces the bug: a test that fails without the fix and passes with it. Seeing
  it fail first is what shows it tests the bug; its comment says what used to go wrong, as
  `TaskExecutorTests.ATaskAddedDuringAnUpdateRunsOnTheNextOne` does.
- Tests follow the code conventions: C++ is formatted with `oui_run tools/format_cpp.js`, and warnings are errors.
- A name scripts can use has an `api` comment; OUI Code's repository makes its API files of them, and its
  `OuiRun.ScriptApi` test fails when one is missing.
- OUI and the programs here use neither C++ streams nor `std::regex`; `OuiLeanSourcesTests` enforces it.
