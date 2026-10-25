# Concurrency and CTTasks

Two small libraries with nothing in common but their size. `concurrency/` keeps a pool of threads that runs work
which can be cancelled, and spreads the elements of a range over them, handing back a `std::future`. `cttasks/` is a
variant of the [tasks library](tasks_library.md) whose task trees are composed at compile time: no heap allocations
and no virtual calls, for small binaries.

- [concurrent_for](#concurrent_for)
- [Reducing to one value](#reducing-to-one-value)
- [How the work is divided](#how-the-work-is-divided)
- [The task pool](#the-task-pool)
- [Cancelling](#cancelling)
- [Latest wins](#latest-wins)
- [CTTasks](#cttasks)
- [CTTasks helpers](#cttasks-helpers)
- [CTTasks compared with Tasks](#cttasks-compared-with-tasks)

## concurrent_for

`concurrency/concurrency.h` (namespace `olagarro::concurrency`) and `concurrency/pool.h`, which it includes, are
header-only. Their CMake target is
`olagarro::Concurrency`, in the build tree and from an installed package alike; it links the
platform's threads. With the module adapters, `import olagarro.concurrency;`.

```cpp
#include <concurrency/concurrency.h>

std::vector<Image> images{load_images()};
auto resizing{olagarro::concurrency::concurrent_for(images.begin(), images.end(),
    [](std::size_t, Image& image) { image = resize(image, 640, 480); })};
// ... other work while the images resize ...
resizing.get();   // waits, and rethrows the first exception a worker threw
```

`concurrent_for(begin, end, function)` calls `function(index, element)` once for every element of `[begin, end)` and
returns a `std::future<void>` that is ready when every call has returned. `index` is the element's position from
`begin`, as a `std::size_t`; `element` is `*iterator`, so with a mutable range it is a reference the function can change
in place, and with a `const_iterator` a const one.

The calls run on several threads at once: the workers of a [task pool](#the-task-pool), and the thread that waits
for the future, which takes its part of the work while it waits. `concurrent_for(pool, begin, end, function)` runs on
the pool given; without one, on `TaskPool::shared()`, the program's.

- **The function object is shared.** All workers call the same copy of it, so its call must be safe to make from
  several threads together. A lambda that writes to something it captured, or a `mutable` lambda with state of its
  own, needs that state guarded or split by index.
- **Writing to separate elements is safe**, and so is writing to slot `index` of a vector sized beforehand. OUI Code's
  Find in Files (`oui_code/project_search.cpp` in [its repository](https://github.com/iganinja/oui_code)) searches each
  file on its own core this way, and joins the results in file order afterwards:

```cpp
std::vector<std::vector<FileMatch>> per_file(request.files.size());
const auto search_file{[&](std::size_t index, const std::filesystem::path& file)
                       { per_file[index] = find_in_file(request.folder, file, search); }};
concurrency::concurrent_for(request.files.begin(), request.files.end(), search_file).get();
```

- **The range must outlive the work.** The iterators are copied into the background work, not the container.
- **Exceptions stop the rest.** The first exception a call throws is kept; every worker stops before its next
  element, so some elements are never visited, and `get()` rethrows that first exception. Later ones are dropped.

The iterators must be random-access (`end - begin`, `begin + n`). An empty range calls nothing.

The work starts on the pool's workers when `concurrent_for` returns. The future is a deferred one
(`std::launch::deferred`): `get()` or `wait()` runs, on the calling thread, the shares no worker has taken yet, then
waits for the ones being run, so a call waited for inside a pool task, or while every worker is busy, still finishes.
Being deferred, `wait_for` and `wait_until` say `std::future_status::deferred` and do no work, and dropping the future
does not wait: the workers go on with their shares, so what the function refers to must outlive them. Both
overloads are `[[nodiscard]]`. For a single operation in the background, submit it to a pool.

## Reducing to one value

The second overload takes a transform and a `Reductor`, and returns a `std::future<Value>`:

```cpp
const std::vector<std::string> words{split_words(book)};
const auto longest{olagarro::concurrency::concurrent_for(words.begin(), words.end(),
    [](std::size_t, const std::string& word) { return word.size(); },
    olagarro::concurrency::reductor(std::size_t{0},
        [](std::size_t longest, std::size_t size) { return std::max(longest, size); }))
    .get()};
```

`reductor(initial_value, combine)` builds the `Reductor<Value, Combine>`; `Value` is the decayed type of the initial
value, which is also the type the future gives. `transform(index, element)` returns a value for each element, and
`combine(result_so_far, next_value)` folds them.

Each worker folds its own share, starting from its first transformed value; when all are done, the partial results are
folded into `initial_value` one after another, in the order of the shares. So:

- **`initial_value` counts once**, whatever the number of workers:
  `ConcurrencyTests.CountsTheInitialValueOnceWhateverTheWorkers` sums 1 to 100 from 10 and expects 5060.
- **`combine` must be associative**: it must give the same result however the values are grouped, as a sum, a
  product, a maximum or a concatenation do. The order of the elements is kept, so it need not be commutative.
- An empty range returns `initial_value`.

## How the work is divided

The range is cut into contiguous shares of nearly equal size: four for each thread that can take them (the pool's
workers and the caller), and no more than there are elements. `concurrent_for` submits one task to the pool for each
worker (fewer if there are fewer shares); each task, and the caller once it waits, takes the next share nobody has
taken and goes through it in order, until none is left. A share that costs more than the others leaves the rest to
the other threads. No thread is started: the pool's are, once.

## The task pool

`TaskPool` (`concurrency/pool.h`) starts its workers once, as many as asked or, by default, as many as
`std::thread::hardware_concurrency()` (at least one), and runs the work submitted to it first in, first out:

```cpp
#include <concurrency/pool.h>

olagarro::concurrency::TaskPool pool;
auto thumbnail{pool.submit([&](const olagarro::concurrency::CancelToken&) { return make_thumbnail(path); })};
// ... other work ...
const Image image{thumbnail.get()};   // waits, and rethrows what the work threw
```

`submit(work)` calls `work(token)` on a worker and gives back a `TaskHandle<Result>`, `Result` being what `work`
returns (`void` too). The handle has `cancel()`, `is_finished()`, `wait()`, `get()` (once), `future()`, the
`std::future<Result>` itself, and `token()`. The work is moved into the pool; it need not be copyable.

`TaskPool::shared()` is the program's pool, started when first asked for and destroyed when the program ends; what
should share the cores shares it, as `concurrent_for` does when given no pool. A pool of its own suits work that must
not wait behind the rest, or a test that needs a known number of workers.

Destroying a pool waits for the work running; what has not started is dropped, and its handle's `get()` throws
`TaskCancelled`. Work submitted while the pool is being destroyed is dropped the same way.

## Cancelling

A `CancelToken` is a shared flag: copies share it, and `cancel()` on any of them cancels all. Each task has one; the
work receives it and checks `cancelled()` where stopping is cheap, between rows of an image or blocks of sound:

```cpp
auto render{pool.submit([](const CancelToken& token) {
    Image image{width, height};
    for(int y{}; y < height && !token.cancelled(); ++ y)
    {
        render_row(image, y);
    }
    return image;
})};
render.cancel();
```

`TaskHandle::cancel()` cancels the task's token. A task cancelled before it starts never runs: its `get()` throws
`TaskCancelled`. A task already running goes on until it checks, and what it returns then is its result; the pool
does not stop it.

## Latest wins

`LatestWins<Request, Result>` runs requests on a pool when only the newest matters, as the frame a viewer shows while
the playhead is dragged. Each request cancels the ones before it, and a result is handed on only if its request was
still the latest when it finished:

```cpp
olagarro::concurrency::LatestWins<int, Image> preview{
    olagarro::concurrency::TaskPool::shared(),
    [&](const int& frame, const CancelToken& token) { return render_frame(project, frame, token); },
    [&](Image image) { application.post([&, image{std::move(image)}] { viewer.show(image); }); }};
preview.request(playhead);   // on every move
```

`on_result` is called on the worker's thread; a window hands the result to its own thread, as above. Of ten requests
made in quick succession only the last is handed on: the others are dropped before they start or stop at their next
check. `cancel()` cancels every request made so far, `wait()` waits until they have finished or been dropped, and
destroying it does both, so neither function is called once it is gone. `request`, `cancel` and `wait` are called
from one thread, and `wait` not from `work` or `on_result`.

## CTTasks

`cttasks/` (namespace `olagarro::ct_tasks`) runs the same kind of task trees as the [tasks library](tasks_library.md),
with the tree's shape fixed at compile time. A composite holds its children in a `std::tuple`, by value, so the whole
tree is one object of one nested type, with no `unique_ptr`, no heap allocation and no virtual function; calls are
resolved statically and can be inlined. CONTRIBUTING.md keeps it that way: `CTTasks` composes statically, without
virtual dispatch or heap allocation.

A task is any copyable type with two members:

```cpp
void on_about_to_start();          // before each run
olagarro::ct_tasks::State update(float delta_time);   // State::working or State::finished
```

There is no base class; a type with those members is a task. `tests/cttasks/containertasks_test.cpp` uses one that
counts its starts in an `int` it refers to:

```cpp
class ValueTestTask
{
  public:
    ValueTestTask(int& value) : m_value{value} {}
    void on_about_to_start() { ++ m_value; }
    State update(float) { return State::finished; }

  private:
    int& m_value;
};
```

Helpers take tasks by value and copy them into the composite. The lifecycle is the tasks library's: `on_about_to_start`
before every run, `update` once per frame until it returns `finished`, and a task that runs again is started again.

`TaskExecutor<Tasks...>` (`cttasks/include/cttasks/taskexecutor.h`) takes its tasks in its constructor, holds them in
a `Parallel`, and starts them there. It has no `add_task`: the set is fixed when it is built. Class template argument
deduction names its type:

```cpp
#include <cttasks/execute.h>
#include <cttasks/serial.h>
#include <cttasks/taskexecutor.h>
#include <cttasks/wait.h>

using namespace olagarro;

ct_tasks::TaskExecutor executor{
    ct_tasks::serial(ct_tasks::wait(1.0f), ct_tasks::execute([&] { door.open(); }))};

executor.update(delta_seconds);   // with the frame's delta
executor.update();                // or measured: the time since the last update() or the construction
```

`update()` without arguments measures the time since the last such call, or since construction, with
`std::chrono::high_resolution_clock`, in whole milliseconds. `update(float)` takes the delta given and does not
touch that clock. Tasks that finished stay in the executor and are not updated again; there is no call that tells
whether all have finished, so a program that needs to know puts an `execute` at the end of a `serial`.

The CMake target is `olagarro::CTTasks`, a static library: the composites are header-only, and `Wait` is compiled in
`cttasks/src/wait.cpp`. With the module adapters, `import olagarro.cttasks;`.

## CTTasks helpers

| Helper | Header | What it does |
|--------|--------|--------------|
| `serial(a, b, ...)` | `serial.h` | One after another; the next starts and is updated in the same frame the previous finishes, with a delta of 0, so that frame's time counts once. |
| `parallel(a, b, ...)` | `parallel.h` | All together; finishes when all have. A finished task is not updated again. |
| `early_exit_parallel(a, b, ...)` | `earlyexitparallel.h` | All together; finishes when the first does, the ones after it not updated that frame. |
| `execute(callable)` | `execute.h` | Calls `void()` once and finishes. |
| `wait(float seconds)` | `wait.h` | Finishes when the deltas add up to `seconds`. |
| `wait(predicate)` | `wait.h` | Finishes on the first update where `predicate()` is true. |
| `repeat(task, times)` | `repeat.h` | Runs `task` `times` times; `times` must not be 0 (an `assert` says so). |
| `repeat_forever(task)` | `repeat.h` | Restarts `task` each time it finishes; never finishes. |
| `repeat_while(predicate, task)` | `repeat.h` | Runs `task` again while `predicate()` holds, checked at the start and after each run. |
| `select(select_entry(predicate, task), ...)` | `select.h` | The first entry whose predicate task finishes runs its task; finishes with it. |

`wait` takes a number of seconds of any arithmetic type, `wait(1.0f)`, `wait(1.0)` or `wait(1)`, or a predicate that
says when the wait is over.

`cttasks/include/cttasks/mputils.h` holds the template machinery the composites share: `visit_tuple_at(tuple, index,
visitor)` calls `visitor(index, element)` on the element at a run-time index, through a chain of comparisons the
compiler unrolls, and `for_each_in_tuple(tuple, visitor)` visits every element in order. A visitor is a generic lambda,
`[](std::size_t, auto& task) { task.on_about_to_start(); }`.

`serial` and `parallel` need at least one task: an empty `serial` fails an `assert` when it starts, and an empty
`parallel` does not compile.

## CTTasks compared with Tasks

| | Tasks (`olagarro::tasks`) | CTTasks (`olagarro::ct_tasks`) |
|-|---------------------------|---------------------------------|
| Tree | `TaskPtr`s on the heap, built at run time | values in tuples, fixed at compile time |
| Calls | virtual | static, inlinable |
| Executor | adds tasks at any time, destroys finished ones | fixed set given to the constructor |
| Helpers | many: selectors, signals, triggers, interpolation, `wait_held`... | the ten above |
| `repeat_while` | checks the predicate on every update, and can cut a run short | checks it at the start and between runs only |
| `select` | updates every predicate on each update until one wins | stops at the first predicate that finishes |
| Binary size | one copy of each composite | one instantiation per tree shape |

CTTasks suits a fixed sequence in a program where every allocation and every byte counts; the tasks library suits
anything built from data, changed at run time, or using its richer helpers.
