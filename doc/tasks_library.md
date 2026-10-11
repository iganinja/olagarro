# The tasks library

The tasks library (`tasks/`, namespace `olagarro::tasks`) describes work that takes time, as a tree of small tasks a
frame loop advances: wait two seconds, then fade a value in while a sound plays, then repeat until a signal fires. Each
task is updated once per frame with the frame's delta time and says whether it is still working or has finished;
composite tasks combine others in sequence, side by side, by choice or in loops. The tree is built once with plain
function calls and owned by a `TaskExecutor`.

This document covers the `Task` lifecycle, the executor, every helper the library provides, how to write a task of
your own, and the C++20 module. For the statically composed variant, which needs no heap and no virtual functions,
see [CTTasks](concurrency.md#cttasks).

- [A first example](#a-first-example)
- [Task and its lifecycle](#task-and-its-lifecycle)
- [TaskExecutor](#taskexecutor)
- [Composites](#composites)
- [Waiting](#waiting)
- [Running code](#running-code)
- [Repeating](#repeating)
- [Choosing](#choosing)
- [Interpolation](#interpolation)
- [Writing your own task](#writing-your-own-task)
- [Deep trees](#deep-trees)
- [Headers, targets and the module](#headers-targets-and-the-module)

## A first example

```cpp
#include <tasks/execute.h>
#include <tasks/interpolate.h>
#include <tasks/parallel.h>
#include <tasks/serial.h>
#include <tasks/taskexecutor.h>
#include <tasks/wait.h>

using namespace olagarro;

tasks::TaskExecutor executor;
executor.add_task(tasks::serial(
    tasks::wait(1.0f),
    tasks::parallel(
        tasks::interpolate_value(0.0f, 1.0f, 0.5f, [&](float alpha) { door.set_alpha(alpha); }),
        tasks::execute([&] { sound.play("door"); })),
    tasks::execute([&] { door.open(); })));

// In the frame loop:
executor.update(delta_seconds);
```

One second after the first update the door fades in over half a second while its sound starts, and then opens.
The executor destroys the tree once it has finished.

## Task and its lifecycle

`Task` (`tasks/include/tasks/task.h`) is the base of every task. Tasks are held as `TaskPtr`, a
`std::unique_ptr<Task>`: whoever holds the pointer owns the task, and a composite owns its children.

A task goes through runs. Each run is:

1. **`on_about_to_start()`**, called once before the run's first update by whoever runs the task: the executor for a
   task added to it, a composite for its children. It calls the protected `task_on_about_to_start()`, where a task
   sets up its run, and marks the task started.
2. **`update(delta_time)`**, called once per frame, with the frame's time in seconds. It calls the protected
   `task_update(delta_time)` and returns its `State`:
   - `Task::State::working`: not done; update it again next frame.
   - `Task::State::finished`: done. `update` clears the started mark, so `was_started()` is `false` again.

A task can run again after it finishes: `repeat` and the selectors start their children many times. Every new run
begins with `on_about_to_start()` again, so whatever a run changes is reset in `task_on_about_to_start()`, not in the
constructor. `Wait` sets its elapsed time to 0 there, `interpolate` asks for its duration again, `make_trigger`'s task
forgets an earlier fire.

`was_started()` tells a runner whether `on_about_to_start()` is still to be called. Composites and the executor use it;
code that builds trees does not need to.

Each task has a name, given to the constructor (`Task{"Serial"}`); `name()` returns it. `tree()`, for debugging,
returns the names of the task and of all it holds, as `children()` reports them, a line each, in their order, each
preceded by two spaces per level of depth. Both functions exist while `TASKS_USE_NAMES` is defined, which `task.h` always does.

## TaskExecutor

`TaskExecutor` (`tasks/include/tasks/taskexecutor.h`) owns top-level tasks and runs them side by side.

| Member | What it does |
|--------|--------------|
| `add_task(TaskPtr&&)` | Takes the task. A null pointer is ignored. |
| `update(float delta_time)` | Starts every task not yet started, updates each once, and destroys those that finished. |
| `clear()` | Destroys every task, those waiting to be added included, without updating them. |
| `is_empty()` | Whether it holds no task, counting those added during an update that wait for the next one. |
| `current_delta()` | The delta of the update in progress; between updates the last one's, and 0 before the first. |

Tasks run in the order they were added. A task added during `update`, by a task's own code (an `execute` that
schedules more work, for example), is not run by that update: it waits in a list of its own and joins the others at
the start of the next `update`. The executor walks its vector of tasks while it updates them, and an addition that
reallocated that vector would leave the walk on freed memory
(`TaskExecutorTests.ATaskAddedDuringAnUpdateRunsOnTheNextOne`).

`clear()` called from a task during `update` waits for that task to return: then the update ends there, the tasks it
had not reached are not updated, and every task goes. Tasks added after it in the same update run in the next.

The destructor calls `clear()`. Finished and cleared tasks are destroyed with `destroy_task_tree`, as described in
[Deep trees](#deep-trees).

`current_delta()` lets code inside a task that does not receive the delta, an `execute` callback for example, read it:

```cpp
executor.add_task(tasks::execute_every_update([&] { clock.advance(executor.current_delta()); }));
```

## Composites

Composites take ownership of other tasks. Null pointers given to them are dropped.

### serial

`serial(a, b, c)` runs its tasks one after another and finishes when the last one does. When one finishes, the next is
started and updated in the same frame with a delta of 0: a chain of `execute` tasks runs all in one update, and what
follows a wait runs in the update the wait ends, but the time of that update is counted once, by the task that had
it, so two waits of a second take two seconds whatever the frames.
An empty list finishes on its first update.

### parallel

`parallel(a, b, c)` starts all its tasks together and updates each one every frame until it finishes; it finishes when
every one has. A task that finished is not updated again during that run.

### early_exit_parallel

`early_exit_parallel(a, b, c)` runs its tasks side by side and finishes as soon as any of them finishes. Tasks are
updated in order, and the update stops at the one that finished: those after it are not updated that frame. Nothing
is told it was interrupted; the others are simply dropped with the group. With no tasks it finishes on its first
update.

`early_exit_parallel_with_sidecars(ends_group, sidecars)` adds tasks that run alongside but never end the group:

```cpp
// Patrol until the player is seen; meanwhile play the footsteps, which may finish on their own.
executor.add_task(tasks::early_exit_parallel_with_sidecars(
    tasks::task_list(tasks::wait([&] { return guard.sees_player(); })),
    tasks::task_list(std::move(patrol), std::move(footsteps))));
```

A sidecar that finishes is not updated again until the group starts again. On the update the group ends, the sidecars
are not updated. With `ends_group` empty, the group finishes on its first update.

`task_list(a, b, c)` builds the `std::vector<TaskPtr>` these functions take. A braced list cannot hold `unique_ptr`s,
since it copies its elements; `task_list` moves them in.

### never_finish

`never_finish(task)` runs `task` until it finishes and then keeps reporting `working` without updating it again. It
keeps a task that has done its job from ending an `early_exit_parallel` or a priority selector's branch:

```cpp
tasks::early_exit_parallel(
    tasks::never_finish(tasks::execute([&] { door.open(); })),   // runs once, and does not end the group
    tasks::wait(3.0f));                                          // this one ends it
```

### Passing tasks to the variadic helpers

`serial`, `parallel`, `early_exit_parallel`, `select` and `priority_selector` take their tasks as arguments and move
them in. Pass temporaries or `std::move(task)`: a named `TaskPtr` passed as it is does not compile, rather than being
emptied without a word. A null task is left out, as `add_task` leaves it; an entry of `select` with no predicate too,
and `repeat` or `repeat_while` with no task finish on their first update.
`serial()` and `parallel()` with no arguments do not compile; `make_serial`, `make_parallel` and
`make_early_exit_parallel` take a `std::vector<TaskPtr>` instead, which may be empty or built at run time.

## Waiting

All in `tasks/include/tasks/wait.h`, except `wait_signal` and `make_trigger`.

| Call | Finishes |
|------|----------|
| `wait(float seconds)` | once the deltas of its updates add up to `seconds`. |
| `wait(predicate)` | on the first update where `predicate()` returns true. |
| `wait_held(predicate, seconds)` | when `predicate()` has held for `seconds` in a row. |
| `wait_for(std::function<float()> duration)` | after `duration()` seconds, asked on every start. |
| `wait_random(min, max, generator)` | after a time drawn between `min` and `max` on every start. |
| `wait_signal(signal)` | on the first update after the signal fires. |
| `make_trigger()` | on the first update after its `Trigger` fires. |

`wait(seconds)` takes the seconds as any arithmetic type, `wait(1.0f)`, `wait(1.0)` or `wait(1)`; anything else is a
predicate. The time is counted from the first update, and the
update that crosses the limit finishes; no leftover time is carried into the next task.

`wait(predicate)` checks the predicate on every update, the first included. A predicate that holds already finishes
on the first update.

`wait_held` adds the delta of each update where the predicate holds, and starts again from zero on an update where
it does not:

```cpp
// The button has to be held for half a second.
tasks::wait_held([&] { return input.is_down(Button::fire); }, 0.5f);
```

`wait_for` asks for the duration at the start of every run, so a repeated wait can change each time.
`wait_random(min, max, generator)` is `wait_for` with a `std::uniform_real_distribution<float>` drawn from
`generator`, any standard random bit generator, which the task does not own: it must outlive the task.

### wait_signal

`wait_signal(signal)` (`tasks/include/tasks/waitsignal.h`) waits for an `olagarro::signal_slot::Signal` to fire (see
[signals](signalslot.md)). It connects to the signal when it starts and disconnects when it finishes, starts again or
is destroyed, so fires before it starts do not count. It keeps the arguments of the first fire after it started;
`arguments()` returns a handle to them, which stays valid after the task is gone, so take it before the task is moved
into a tree:

```cpp
signal_slot::Signal<int, const std::string&> chosen;
auto waiting{tasks::wait_signal(chosen)};
const auto arguments{waiting->arguments()};
executor.add_task(tasks::serial(std::move(waiting),
                                tasks::execute([arguments] { use(std::get<0>(arguments.get())); })));
```

`arguments.received()` says whether it fired; `arguments.get()` is a `std::tuple` of the decayed parameter types,
valid only when it did. The signal is not owned. It must outlive the task's last start; a signal destroyed after that
leaves the task waiting forever, never touching freed memory. This header is the only part of the library that needs
the signals: whoever includes it links `olagarro::SignalSlot` too.

### make_trigger

`make_trigger()` (`tasks/include/tasks/trigger.h`) returns a task and its `Trigger`. `trigger.fire()` makes the task
finish on its next update:

```cpp
auto [task, trigger] = tasks::make_trigger();
executor.add_task(tasks::serial(std::move(task), tasks::execute([] { show_menu(); })));
// Later, from anywhere:
trigger.fire();
```

The trigger shares its state with the task, so it can be copied and kept; firing it after the task is gone does
nothing. Starting the task forgets an earlier fire: a fire counts only from the start of the run on, and firing
before the executor's first update, which starts the task, is lost.

## Running code

`tasks/include/tasks/execute.h`:

| Call | What the callable is | When it finishes |
|------|----------------------|------------------|
| `execute(callable)` | `void()`, called once | on its first update, the call made |
| `execute_every_update(callable)` | `void()`, called on every update | never |
| `execute_until(callable)` | `Task::State(float delta)`, called on every update | when the callable returns `Task::State::finished` |

```cpp
// Moves the camera until it arrives.
tasks::execute_until([&](float delta) {
    camera.move_towards(target, speed * delta);
    return camera.position() == target ? tasks::Task::State::finished : tasks::Task::State::working;
});
```

The callable is copied into the task and called as often as the task is updated, run after run.

## Repeating

`tasks/include/tasks/repeat.h`:

- `repeat(task, times)` runs `task` `times` times, one run after another, however many updates each run takes, and
  then finishes. When a run finishes, the next is started at once and first updated on the next frame. Started again,
  it counts from the beginning. `repeat(task, 0)` finishes on its first update.
- `repeat_while(predicate, task)` checks `predicate()` at the beginning of every update, before the task. While it
  holds, it updates the task, and restarts it on the update after it finishes. When it no longer holds,
  `repeat_while` finishes at once, in the middle of a run if the task was in one. It never finishes on its own.
- `repeat_forever(task)` is `repeat_while` with a predicate that always holds.

```cpp
// Blink while the alarm is on.
tasks::repeat_while([&] { return alarm.on(); },
    tasks::serial(tasks::execute([&] { light.toggle(); }), tasks::wait(0.25f)));
```

## Choosing

### select

`select(SelectEntry{predicate, task}, ...)` (`tasks/include/tasks/select.h`) chooses once. The predicates are tasks,
usually `wait(...)`; on every update each of them is updated, and the first entry, in order, whose predicate finished
is chosen. The chosen task is started then and updated from the next update on; `select` finishes when it does.
Until a predicate finishes, `select` keeps working, and once a task is chosen the predicates are no longer updated.

```cpp
tasks::select(tasks::SelectEntry{tasks::wait([&] { return door.locked(); }), tasks::execute([&] { knock(); })},
              tasks::SelectEntry{tasks::wait(2.0f), tasks::execute([&] { door.open(); })});
```

Every predicate is updated on each update, even after an earlier one has finished, so predicates with effects all
run. Entries without a task are dropped. With no entries, `select` never finishes.

### priority_selector

`priority_selector(main_predicate, PriorityEntry{predicate, task}, ..., otherwise(task))`
(`tasks/include/tasks/priorityselector.h`) picks again on every update, the way behaviour trees choose what an agent
does:

- The main predicate, a task, is updated first; when it finishes, the selector finishes. With `nullptr` as the main
  predicate the selector never finishes.
- The entries' predicates are then updated in order. The first one that finishes wins, and those after it are not
  updated that frame.
- If the winner is not the entry that was running, the running entry is dropped: its predicate is started again, and
  its task will start afresh the next time it wins. The winner's task is started.
- The winner's task is updated. When it finishes, its predicate is started again, and the task starts again the next
  update its entry wins.
- `otherwise(task)` is an entry whose predicate always holds, for the last place: its task runs when no other entry's
  predicate finishes.

```cpp
executor.add_task(tasks::priority_selector(
    tasks::wait([&] { return enemy.dead(); }),
    tasks::PriorityEntry{tasks::wait([&] { return enemy.health() < 20; }), flee()},
    tasks::PriorityEntry{tasks::wait([&] { return enemy.sees_player(); }), attack()},
    tasks::otherwise(patrol())));
```

A dropped task is not told it was interrupted; it is simply not updated any more. Wrapping a task in `never_finish`
keeps its entry running after it is done, instead of restarting it. Entries without a predicate or a task are
dropped.

## Interpolation

`tasks/include/tasks/interpolate.h` changes values over time.

- `interpolate(seconds, apply, curve)` calls `apply(float)` on every update with `curve(u)`, where `u` is the time
  since the start over `seconds`, this update's delta included, so the first update already moves. The update the
  time runs out on calls `apply(1.0f)` exactly, without the curve, and finishes; a duration of 0 or less does so on
  the first update.
- `interpolate_value(from, to, seconds, setter, curve)` calls `setter(from + (to - from) * curve(u))` and, on the last
  update, `setter(to)` exactly, since the arithmetic may not land on it. `Value` is any type with `+`, `-` and `*` by a
  float: a float, a vector, a colour.
- `progress_monitor(seconds, curve)` returns a task and a `Progress`. The task applies nothing; it only measures, and
  `progress.value()` reads how far it is, from 0 to 1, and exactly 1 once it is over. The `Progress` shares its state
  with the task, so it stays valid after the task is gone, keeping its last value; it goes back to 0 at every start. A
  second overload takes a `std::function<float()>` duration, asked on every start.

```cpp
auto [timer, progress] = tasks::progress_monitor(2.0f);
executor.add_task(std::move(timer));
// Anywhere, every frame:
bar.set_fraction(progress.value());
```

The curves are in `tasks::curves`, each mapping 0 to 0 and 1 to 1: `linear` (the default), `ease_in` (`u * u`),
`ease_out` (`u * (2 - u)`), `ease_in_out`, and `smoothstep` (`u * u * (3 - 2 * u)`). Any `float(float)` callable
serves as a curve.

The class behind them is `Interpolate<Duration, Apply, Curve>`; its duration is a callable, asked on every start, so
a repeated interpolation can change its length.

## Writing your own task

Derive from `Task`, give it a name, and override the two protected functions:

```cpp
class Blink final : public tasks::Task
{
  public:
    Blink(Light& light, int times) : Task{"Blink"}, m_light{light}, m_times{times} {}

  protected:
    // Called before every run: reset what a run changes.
    void task_on_about_to_start() override
    {
        m_toggles = 0;
        m_elapsed = 0.0f;
    }

    State task_update(float delta_time) override
    {
        m_elapsed += delta_time;
        if(m_elapsed < 0.25f)
        {
            return State::working;
        }
        m_elapsed = 0.0f;
        m_light.toggle();
        return ++ m_toggles == m_times * 2 ? State::finished : State::working;
    }

  private:
    Light& m_light;
    int m_times;
    int m_toggles{0};
    float m_elapsed{0.0f};
};

executor.add_task(std::make_unique<Blink>(light, 3));
```

`task_update` is pure virtual; `task_on_about_to_start` does nothing unless overridden. The public `on_about_to_start`
and `update` keep the started mark and are left alone.

A task that holds others, a composite, also:

- starts its children with `child->on_about_to_start()` before their first update, and again before each new run,
  and updates them with `child->update(delta_time)`;
- overrides `children()`, returning a pointer to each child, so `tree()` shows them;
- overrides `grab_children()`, moving every child out into the vector it returns, so trees are destroyed without
  recursion (next section).

`RepeatWhile` in `tasks/include/tasks/repeat.h` is a short composite to copy from; `tests/tasks/probe.h` is a minimal
leaf task that counts its starts and updates.

## Deep trees

Destroying a `unique_ptr` destroys what it owns, and a composite's destructor destroys its children: a tree tens of
thousands of levels deep, as a loop that wraps a task in `serial` again and again can build, would overflow the stack.
`destroy_task_tree(TaskPtr)` destroys a tree without recursion: it keeps a vector of pending tasks, takes each one's
children with `grab_children()` before letting it go, and so destroys one task at a time, each already emptied. The
executor destroys finished and cleared tasks this way (`TaskExecutorTests.DestroysDeepTaskTreesIteratively` builds a
tree 20 000 levels deep).

This is why every composite must override `grab_children()`. One that does not still works, but its subtree is
destroyed recursively, by its destructors. A tree destroyed by simply resetting its `TaskPtr`, instead of through an
executor, is destroyed recursively too; call `destroy_task_tree(std::move(task))` for one that may be deep.

## Headers, targets and the module

Each part has its own header under `tasks/include/tasks/`, included as `<tasks/serial.h>`, `<tasks/wait.h>` and so
on; `<tasks/taskscore.h>` includes serial, parallel, early exit parallel, wait, execute and repeat together. The CMake
target is `olagarro::Tasks`, a static library built from `tasks/src/`; only `waitsignal.h` needs
`olagarro::SignalSlot` beside it. The library is C++17.

With `OLAGARRO_BUILD_MODULES=ON` (CMake 3.28 or newer and a module-aware generator), `modules/olagarro.tasks.cppm`
builds a C++20 module adapter over the same headers:

```cpp
import olagarro.tasks;   // or import olagarro; for every library

olagarro::tasks::TaskExecutor executor;
executor.add_task(olagarro::tasks::serial(olagarro::tasks::wait(1.0f), olagarro::tasks::execute([] {})));
```

It exports the classes (`Task`, `TaskExecutor`, `TaskPtr`, `Trigger`, `Progress`, `SignalArguments`,
`SelectEntry`, `PriorityEntry` and the task class templates), the functions listed in this document, and
`tasks::curves`. Its target is `olagarro::ModuleTasks`, and it links the signals for `wait_signal`. The library itself stays C++17;
the adapter is never part of the default build.
