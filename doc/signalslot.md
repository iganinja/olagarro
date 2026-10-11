# Signals and slots

`signalslot/signalslot.h` is a header-only library of typed signals: an object fires a `Signal<int>`, and every
callback connected to it is called with the value. OUI uses it for everything a widget tells the code that holds it,
`LineEdit::on_changed` or `Window::on_closed`. This document covers connecting, disconnecting, what happens when the
list of callbacks changes during a fire, who owns what, and why the code is written as it is.

- [A first signal](#a-first-signal)
- [Connections](#connections)
- [Firing](#firing)
- [Changes while firing](#changes-while-firing)
- [Lifetime](#lifetime)
- [Slot objects](#slot-objects)
- [How it is built](#how-it-is-built)

Everything is in the namespace `olagarro::signal_slot`. The CMake target is `olagarro::SignalSlot`, in the build
tree and from an installed package alike; with the module adapters, `import olagarro.signalslot;`.

## A first signal

A `Signal` is declared with the types of what it carries, and any callable that takes those connects to it:

```cpp
#include <signalslot/signalslot.h>

using namespace olagarro;

signal_slot::Signal<std::string_view> changed;

const auto connection{changed.connect([](std::string_view text) { std::printf("now %s\n", text.data()); })};
changed.fire("hello");      // calls the lambda
connection.disconnect();
changed.fire("again");      // calls nothing
```

A class usually keeps the signal private and offers a function that connects to it, as OUI's widgets do
(`oui/include/oui/line_edit.h`):

```cpp
template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
{
    return m_changed.connect(std::forward<Callback>(callback));
}

olagarro::signal_slot::Signal<std::string_view> m_changed;
```

A signal can carry nothing (`Signal<>`), several values (`Signal<std::filesystem::path, bool>`), or references
(`Signal<const Widget&, int>`, as `DockLayout` has).

## Connections

`Signal::connect(callback)` takes any callable whose call accepts the signal's parameters: a lambda, a function
object, a function pointer. It is copied or moved into the signal (one that can only be moved, such as a lambda that
owns a `unique_ptr`, is moved), and the signal keeps it until it is disconnected or the signal is destroyed.

`connect` returns a `Connection`, a small handle to that one callback:

| Call | What it does |
|------|--------------|
| `connection.disconnect()` | Disconnects the callback. Doing it again, or after the signal is gone, does nothing. |
| `connection.is_connected()` | Whether the callback is still connected; `false` once the signal is gone. |

Ignoring the `Connection` leaves the callback connected for as long as the signal lives. That is the right choice when
the callback and the signal belong to the same object, or the callback captures nothing that may go first.

A default-constructed `Connection` refers to nothing; `disconnect` and `is_connected` on it are harmless. Copies of a
`Connection` refer to the same callback.

### ScopedConnection

`ScopedConnection` disconnects its callback when it is destroyed. It is the usual way to tie a callback to the
lifetime of whatever it captures:

```cpp
// examples/oui_code/documents.h: what a running script session holds.
olagarro::signal_slot::ScopedConnection code_changed;

// examples/oui_code/tools_commands.cpp: the session hears the editor while it lives, and no longer.
session->code_changed = signal_slot::ScopedConnection{session->view->code().on_changed(soon)};
```

It can be moved but not copied. Assigning another to it disconnects the callback it had before taking the new one,
so a member reassigned on every refresh never leaves old callbacks behind. `disconnect()` disconnects early. A
`std::vector<ScopedConnection>` holds several (`examples/oui_desktop/desktop_area.h` keeps its connections so).

## Firing

`fire(parameters...)` calls, in the order they were connected, first the [slot objects](#slot-objects) and then the
callbacks. Parameters are taken by value as the signal declares them, so a `Signal<const Widget&>` passes a
reference and a `Signal<std::string>` copies the string for each callback.

A callback that throws stops the fire: the exception leaves `fire`, the callbacks after it are not called that time,
and the signal is left as it was, every callback still connected and nothing leaked
(`SignalSlotTests.ACallbackThatThrowsLeavesNothingBehind`).

Firing makes no copies of the callbacks; each is called where it is stored.

## Changes while firing

A callback can do anything to the signal that fired it while it runs, and the rules are these:

- **A fire goes through the callbacks that were connected when it started.** A callback connected during a fire is
  called from the next fire on, not by the one in progress.
- **A callback disconnected during a fire is not called after that,** even if its turn in this fire has not come.
- **A callback may disconnect itself.** It is no longer connected, but it is not destroyed while it runs: the signal
  only marks it, and destroys every marked callback when the outermost fire ends. Its captures stay valid until it
  returns.
- **Fires nest.** A callback can fire the same signal again; the inner fire follows the same rules, and callbacks
  disconnected inside it wait for the outer fire to finish before they are destroyed.
- **`disconnect_all()` during a fire** stops every remaining call of that fire and of the fires around it.
- **A callback may destroy the signal.** The fire holds the signal's state alive until it ends: the callback that
  destroyed it finishes normally, the ones after it are no longer connected and are not called, and they are all
  destroyed when the fire returns.

```cpp
signal_slot::Connection once;
once = clicked.connect([&once] {
    start_download();
    once.disconnect();   // safe: this lambda is kept until it returns
});
```

Each rule has a test in `tests/signalslot/main.cpp`, and the same file is built a second time with AddressSanitizer
and UndefinedBehaviorSanitizer as `Olagarro.SignalSlotSanitized`, which fails if a callback is destroyed twice, used
after it is destroyed, or never destroyed (see [testing](testing.md)).

## Lifetime

- **The signal owns its callbacks.** Each is destroyed exactly once: when it is disconnected (or, during a fire, when
  the fire ends), or with the signal.
- **A `Connection` does not keep the signal alive.** It holds a weak reference to the signal's state, so it may
  outlive the signal; using it then does nothing.
- **The callback is not told when the objects it captures go.** A lambda that captures `this` must be disconnected
  before `this` is destroyed: keep a `ScopedConnection` as a member of the object the lambda captures.
- **Signals cannot be copied or moved.** Connections refer to one signal, and a copy would leave them pointing at the
  wrong one. A class that holds a signal is not copyable either, unless it declares how.

## Slot objects

Besides callbacks, a signal can call objects derived from `Slot<Parameters...>`, which override
`void call(Parameters...)`. A slot remembers the signals it is connected to, and disconnects from all of them when it
is destroyed; a signal disconnects every slot when it is destroyed. Three ready-made kinds exist:

| Class | Calls |
|-------|-------|
| `MethodSlot<Instance, Parameters...>(instance, &Instance::method)` | a member function on an object |
| `FunctionSlot<Parameters...>(function)` | a free function pointer |
| `FunctorSlot<Function, Parameters...>(function)` | any callable it holds |

Each takes the signal as an optional last constructor argument, or connects later with `slot.connect(signal)`;
`slot.disconnect(signal)` and `slot.disconnect_all()` undo it. Connecting a slot to a signal twice connects it once.

```cpp
Counter counter;
signal_slot::Signal<int> signal;
signal_slot::MethodSlot<Counter, int> method_slot{counter, &Counter::add, signal};
auto functor_slot{signal_slot::FunctorSlot{[&counter](int value) { counter.total += value * 10; }, signal}};
signal.fire(2);   // counter.add(2), then the lambda
```

A slot may disconnect itself, or another slot, during a fire: each slot is checked again before it is called. Slots
are called before callbacks, and do not have the protection callbacks have against the signal being destroyed during
the fire. OUI does not use slot objects; `connect` with a callback and a `ScopedConnection` does the same with less.

## How it is built

The library is one header, and its design follows from what OUI asks of it: hundreds of signals, many with lambdas of
their own types, in programs that are built for size.

**Type erasure without virtual functions.** A callback is stored as `detail::Held<Function, Parameters...>`, which
derives from `detail::Invocable<Parameters...>` and `detail::Callable`. Instead of virtual functions they carry two
plain function pointers, made by the template for each callback type: one that calls it with the signal's
parameters, one that deletes it. The comment beside `Callable` gives the reason: a class with virtual functions brings
a table and the type's name (for RTTI) for every lambda, and signals have hundreds of different lambdas. A
`std::unique_ptr<Callable, Destroy>` owns each one, and `Destroy` calls the stored function.

**One core for every signal.** `detail::SignalCore` holds the callbacks and the slots for all signals, whatever they
carry: callbacks as `Callable*`, slots as `void*`. Only `Signal<Parameters...>` knows their types and casts them back.
The code that adds, removes, marks and sweeps is therefore compiled once, not once per signal type.

**Shared state, weak handles.** A signal holds its core in a `std::shared_ptr`; each `Connection` holds a `weak_ptr`
and an id. That is what lets a connection outlive the signal, and lets `fire` keep a strong reference to the core for
its whole length, so a callback can destroy the signal under it.

**Marking instead of erasing during a fire.** Each entry has a `connected` flag. `SignalCore::Firing`, a guard
created by `fire`, counts the fires in progress; while that count is above zero, `disconnect` and `clear` only clear
the flag, and the guard's destructor sweeps the marked entries when the count returns to zero, even when a callback
throws. Outside a fire, `disconnect` erases at once. New callbacks only ever go at the end of the vector and nothing
is erased during a fire, so `fire` walks it by index up to the count it saw at the start, which is why a callback
connected during a fire waits for the next one.
