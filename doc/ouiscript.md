# OUIScript for C++ developers

OUIScript runs JavaScript inside an OUI program, through QuickJS-ng. This document is for the C++ side: how to run
scripts, give them functions to call, hand values back and forth, and keep a script from hanging the program; the
calls every scripting program shares; the highlighters scripts define; how `oui_run` builds its `oui` object; and how
the API reference is generated from comments. Writing scripts is [Scripting](scripting.md)'s subject, and every name
a script can use is in [the API reference](oui_api.md).

- [The pieces](#the-pieces)
- [ScriptEngine](#scriptengine)
- [Values](#values)
- [Promises settled from C++](#promises-settled-from-c)
- [Time limits](#time-limits)
- [Errors](#errors)
- [Unhandled rejections](#unhandled-rejections)
- [SystemApi: the calls every program shares](#systemapi-the-calls-every-program-shares)
- [Highlighting rules](#highlighting-rules)
- [How oui_run's host is built](#how-oui_runs-host-is-built)
- [The api comments and the generated reference](#the-api-comments-and-the-generated-reference)
- [Adding a call or a widget to oui_run](#adding-a-call-or-a-widget-to-oui_run)

## The pieces

| Header | What it is |
|--------|------------|
| [script_engine.h](../oui/include/oui/script/script_engine.h) | `ScriptEngine`, `Value`, `ScriptError`, `ScriptStop` |
| [system_api.h](../oui/include/oui/script/system_api.h) | `SystemApi`: files, programs, timers, questions, clipboard |
| [rule_highlighter.h](../oui/include/oui/script/rule_highlighter.h) | `RuleHighlighter`: colouring by states of regular expression rules |
| [script_highlighter.h](../oui/include/oui/script/script_highlighter.h) | `highlighter_of`: a `RuleHighlighter` from a script's states |

They build into the library `olagarro_oui_script` (CMake target `olagarro::OUIScript`, the component
`OUIScript` of an installed package), which links OUI publicly and QuickJS privately: nothing of QuickJS shows in the headers. The sources are
in `oui/src/script`.

Three programs host scripts, and each is a working example of a different use:

- `examples/oui_run`: a script is the whole program. `Host` ([host.cpp](../examples/oui_run/host.cpp)) gives it
  `oui`, built from native calls and [prelude.js](../examples/oui_run/prelude.js).
- OUI Code's language and command scripts: `Scripting` ([scripting.cpp](../examples/oui_code/scripting.cpp)) runs
  them once at start and keeps the functions they register to call later.
- OUI Code's Run Script and Interactive Script: `ScriptRun` ([script_run.cpp](../examples/oui_code/script_run.cpp))
  and `LiveScript` ([live_script.cpp](../examples/oui_code/live_script.cpp)) load a module, call the function it
  exports, and wait for what it started. See [OUI Code's architecture](oui_code.md#scripting).

## ScriptEngine

A `ScriptEngine` is one QuickJS runtime and context. It runs on the thread that made it, and every call into it must
come from that thread; work that ends elsewhere, as a process's exit, comes back through `Application::post`. Scripts
run in strict mode.

```cpp
olagarro::oui::script::ScriptEngine engine;
engine.set_print([](std::string_view line) { std::fputs((std::string{line} + "\n").c_str(), stdout); });
engine.define("app.greet",
              [&engine](const std::vector<olagarro::oui::script::Value>& arguments)
              { return engine.string("Hello, " + arguments.at(0).to_string()); });
engine.run(R"(console.log(app.greet("terminal"));)", "hello.js");
```

- `run(source, name)` evaluates a classic script. `name` is what error messages and stack traces call it. What it
  leaves on the global object stays for the next `run`: OUI Code's built-in `prelude.js` adds `oui.words`, and the
  language scripts run after it use it.
- `run_module(source, path)` evaluates a module. `import` works from it: a relative import resolves from the folder of
  `path` (QuickJS's own normaliser handles `./` and `../`), and imported modules are read from disk by the engine's
  loader. `path` need not exist: `ScriptRun::load` runs a small module named `<script>.run.js` beside the user's
  script, which imports it, so the script's own imports resolve from its folder. Top-level `await` works; a module
  that is still waiting when `run_module` returns goes on as what it awaits settles. A module that has already failed
  makes `run_module` throw.
- `define(path, function)` makes a C++ function callable as `path`, creating the objects on the way:
  `define("oui.languages.register", ...)` makes `oui` and `oui.languages` if they are not there. The function gets the
  arguments as `Value`s and returns a `Value`; returning `Value{}` gives `undefined`. Defined functions are kept in a
  table in the engine, and each JavaScript function finds its own by its index, QuickJS's "magic" number.
- `set_print` takes what `console.log` writes, one call per line, the values joined by spaces. The constructor
  defines `console.log`; without `set_print` it prints nothing.

The engine is not copyable. The values it hands out keep its runtime alive (see [Values](#values)), so a `Value`
outliving the engine is safe, if useless.

## Values

A `Value` is a JavaScript value held from C++. Copies share it, and it lives while any copy does; a default-made
`Value` is `undefined`.

| To ask | Calls |
|--------|-------|
| What it is | `is_undefined`, `is_null`, `is_bool`, `is_number`, `is_string`, `is_object`, `is_array`, `is_function`, `is_regexp` |
| As JavaScript converts it | `to_string` (`String(v)`), `to_bool` (`Boolean(v)`), `to_number` (`Number(v)`, `NaN` when it fails) |
| A property | `get("name")`: `undefined` when there is none, or when the value is not an object |
| An array | `length()`, `at(index)` |
| An object's names | `keys()`: its own enumerable string properties, in order |
| A call | `call(arguments)`: runs a function under the time limit, throws `ScriptError` when it throws |

The engine makes the simple values: `string`, `number`, `boolean`, `undefined`, `null` and `array(elements)`. There is
no call to make an object from C++. A native function that has several things to give returns an array, and a
JavaScript wrapper turns it into what the script sees: `Host::describe` returns `[number, rootId, rootKind, [id,
widget, kind]...]`, which prelude.js's `Ui` constructor takes apart. The reverse direction is easy: objects a script
passes are read with `get`, as `SystemApi::run_program` reads `{cwd, input, output}`.

Strings cross as UTF-8 both ways. JavaScript counts places in a string in UTF-16 units, so a C++ function that takes
or gives a position in a text converts; oui_run's host does it with `js_index_of` and `byte_of`.

## Promises settled from C++

A native function that cannot answer at once returns a promise and settles it later:

```cpp
auto pending{engine.promise()};           // {promise, resolve, reject}, all Values
// ... later, on the engine's thread:
pending.resolve.call({engine.number(code)});
return pending.promise;                   // what the script awaits
```

`resolve` and `reject` are ordinary functions, so settling one is a `Value::call`, which also runs the jobs the
settling queued: the code after the script's `await` runs inside that call. That is why the code that settles a
promise catches what the call throws, as `SystemApi::later` does:

```cpp
// A promise settled with what `work` gives once the call that asked has returned, as a dialog must be: it
// waits on the user, and a script must not.
template <typename Work> Value SystemApi::later(Work work)
{
    auto pending{m_engine.promise()};
    ++ m_pending;
    m_application.post(
        [this, alive{m_alive}, pending, work]
        {
            if(!*alive)
            {
                return;
            }
            -- m_pending;
            try
            {
                static_cast<void>(pending.resolve.call({work()}));
            }
            catch(const ScriptStop&)
            {
                // Ended by itself, on purpose: nothing to tell.
            }
            catch(const ScriptError& error)
            {
                m_hooks.report(error.what());
            }
        });
    return pending.promise;
}
```

`m_alive` is a `shared_ptr<bool>` the destructor clears: work posted to the application can run after the object
that posted it is gone, and must then do nothing. `SystemApi`, `ScriptRun` and `LiveScript` all use the pattern.

## Time limits

Every `run`, `run_module` and `Value::call` runs against a deadline, 5 seconds unless `set_time_limit` says otherwise.
QuickJS's interrupt handler checks the clock, and a script still running past it is stopped with an uncatchable error,
which reaches C++ as a `ScriptError`. The clock starts at the outermost call: a native function that calls back into
a script does not get a fresh limit. After the outermost call returns, the engine runs the pending jobs (promise
reactions) still against the same clock, then reports the rejections nobody handled.

What each host chooses:

| Host | Limit | Why |
|------|-------|-----|
| oui_run (`Host`) | a year | a script is a program and runs as long as it needs |
| OUI Code's `Scripting` | 5 s (the default) | a text command that loops must give the editor back |
| `ScriptRun` (Run Script) | 30 s | "long enough for real work, short enough that a script stuck in a loop gives the editor back" |
| `LiveScript` (Interactive Script) | 1 s, and 2 s in all for what it waits on | it runs again at every pause in typing |

The limit is per call, not per script: a script that waits on timers or programs is not running while it waits.
`LiveScript` adds a timer of its own for that, and reports "still running after two seconds".

## Errors

`ScriptError` is a `std::runtime_error` whose message is the JavaScript error as `String(error)` would give it, then
its stack, which names the place as `file:line`. `LiveScript` reads the line back from it to send the user there.

A native function reports a failure by throwing: any `std::exception` it throws, `ScriptError` included, becomes a
`TypeError` in the script with the exception's message, which the script can catch. A native function that wants
the script to end there, as `oui.exit` does, throws `ScriptStop`:

```cpp
define("exit",
       [this](const Arguments& a) -> Value
       {
           m_exit_code = static_cast<int>(argument(a, 0).to_number());
           quit();
           throw ScriptStop{};
       });
```

`ScriptStop` derives from `ScriptError`. The engine turns it into an uncatchable error, so no `catch` or `finally` of
the script runs on the way out, and turns it back into a `ScriptStop` when it reaches C++. Callers catch it before
`ScriptError` and say nothing, as nothing went wrong. Once a script has stopped itself the engine remembers it: a
later uncatchable error comes out as `ScriptStop` too.

## Unhandled rejections

A promise rejected with no handler, as an `async` function that throws after an `await`, would otherwise go unseen.
`set_unhandled_rejection(report)` receives its error, with its stack. A rejection is reported only when it is still
unhandled after the outermost call and its jobs have run, since a `catch` attached later in the same work still
handles it. A module whose top level fails is reported once, as the exception `run_module` throws, not again as a
rejection.

oui_run reports them as errors (exit code 1); `ScriptRun` writes them in the run's output tab; `LiveScript` ends the
run with them as its error.

## SystemApi: the calls every program shares

`SystemApi` defines what any program's scripts reach of the system, so oui_run and OUI Code's Run Script and
Interactive Script behave the same: `oui.readFile`, `writeFile`, `exists`, `listDir`, `run` (a shell command, with
`.stop()` on its promise), `message`, `confirm`, `prompt`, `choose`, `openFile`, `saveFile`, `chooseFolder`, `copy`,
`paste`, `openExternally`, and the globals `setTimeout`, `setInterval`, `clearTimeout` and `clearInterval`.

It defines plain native functions as `__ouiSystemNative.*`, then runs a small prelude of its own
([system_api.cpp](../oui/src/script/system_api.cpp)) that wraps them (converting arguments with `String(...)`,
giving defaults, adding `stop` to `run`'s promise), deletes `__ouiSystemNative`, and leaves the result in
`globalThis.__ouiSystem`. The program's own prelude takes that object, deletes it, and spreads it into its `oui`:

```js
// examples/oui_code/script_run.cpp's prelude, all a Run Script sees
const system = globalThis.__ouiSystem;
delete globalThis.__ouiSystem;
globalThis.oui = Object.freeze({...system});
```

A program makes one with its engine, its application and `Hooks`, from `Host::Host`:

```cpp
m_system = std::make_unique<SystemApi>(m_engine, m_application,
                                       SystemApi::Hooks{[this](std::string_view text, bool error)
                                                        { print_output(text, error); },
                                                        [this](std::string_view error) { report(error); },
                                                        [this] { return m_window != nullptr; }, "oui_run"});
```

| Hook | What it is for |
|------|----------------|
| `print(text, error)` | a program's output when the script gave no `output` function; `error` for its standard error |
| `report(error)` | a script function that threw when called back: a timer's, a program's `output`, a settled promise's reaction |
| `has_screen()` | whether questions can open dialogs; without a screen they are asked on the terminal, read from standard input |
| `title` | the title of the dialogs that ask yes or no |
| `read_only` | only looks: see below |
| `writable` | with `read_only`, the one file `writeFile` may still write |

With `read_only`, `run`, `writeFile`, `message`, `confirm`, `prompt`, `choose`, the three file dialogs, `copy` and
`openExternally` throw "oui.X does nothing while the script runs live; run it on its own". That is for Interactive
Script, whose script runs again at every pause in typing, half written: it must not change anything. `writable` is the
exception for the copy of the document a `processFile` works on there, which `LiveScript` makes in a temporary folder.

Two private helpers decide how a question answers:

- `settled(value)` gives a promise already resolved. A question asked on the terminal blocks on standard input and
  answers at once, so the call can wait.
- `later(work)` gives a promise settled with what `work` returns once the current call has returned, posted to the
  application. A dialog runs its own modal loop, which must not start inside a script's call: the script would be
  blocked inside QuickJS while other events, and other callbacks into the same engine, run.

The rest of the class:

- `pending()` counts what scripts started and still wait on: running programs, active timers, open questions. oui_run
  keeps running while it is above zero; `LiveScript` and `ScriptRun` use their own completion signals instead.
- `stop_programs()` stops every program the scripts started; the scripts get their exit codes. OUI Code's output tab
  has a button for it.
- `call(function, arguments)` calls a script's function and reports, through `report`, what it throws.

Programs run through `start_process` with `shell_command` (`/bin/sh -c`, or `cmd.exe /d /s /c` on Windows); their
output and their end come back on the application's thread. A finished program, or a stopped timer, is erased from
its map only after the current call has returned, as it may be inside its own callback.

## Highlighting rules

`RuleHighlighter` implements OUI's `SyntaxHighlighter` with rules written as states, the model TextMate grammars and
Ace use. OUI Code's languages and oui_run's `TextEdit#highlight` both go through it.

```cpp
using namespace olagarro;
// Each rule: pattern, flags, kind, the state it enters, whether it leaves this one.
const oui::script::RuleHighlighter highlighter{{
    {"start", oui::SyntaxKind::plain,
     {{R"(\b(?:if|else|return)\b)", "", oui::SyntaxKind::keyword, "", false},
      {R"(")", "", std::nullopt, "string", false}}},
    {"string", oui::SyntaxKind::string,
     {{R"(\\.)", "", std::nullopt, "", false}, {R"(")", "", std::nullopt, "", true}}},
}};
```

How it colours a line:

- At each place, every rule of the current state is searched from there; the match that starts first wins, and on a
  tie the rule earlier in the list.
- Text no rule matches has the state's kind. A rule's match has the rule's kind, or with none, the kind of the state it
  enters (for a `push`) or of the state it is in. Spans are byte offsets; neighbours of one kind merge, and plain text
  gets no span.
- `push` enters a state by name; `pop` returns to the one before. A rule may do both, pop first. States nest, so a
  string can open inside a template inside a string, up to 64 deep.
- The stack of states is what a line hands to the next. Each different stack is numbered once, and that number is the
  `SyntaxHighlighter::State` the next line starts in; the text's first line starts in `0`, the first state alone.
- A match of nothing that changes no state is skipped, so `/$/` cannot hold a line in place. A rule can still match
  at the end of the line, to change state there. More than 32 state changes without moving leave the rest of the line
  as it is, against rules that go round in circles.
- Patterns are compiled by `oui::Regex`, which is QuickJS's own regular expression engine run from C++: a pattern
  means exactly what it means in a script, and positions are UTF-16 units of the line.

The constructor throws `std::invalid_argument` for a pattern that does not compile or a `push` to a state that does not
exist.

`highlighter_of(states, where)` builds one from a script's value:

```js
{start: [{match: oui.words(["if", "else"]), kind: "keyword"}, {match: /"/, push: "string"}],
 string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}]}}
```

A state is a list of rules, or `{kind, rules}` for one whose unmatched text has a kind. `match` is a `RegExp` (its
`source` and its flags, without `g`, which means nothing here) or a string. Kinds are named as
`syntax_kind_named` reads them: `keyword`, `type`, `function`, `string`, `number`, `constant`, `comment`,
`preprocessor`, `operator`, `tag`, `attribute`, `heading`, `emphasis`, `link`, `invalid`, `plain`. The text starts in
`start`, or in the first state when none has that name. Every error is a `ScriptError` that begins with `where` and
names the state and the rule, so a user's language script says what is wrong: OUI Code passes `language <id>`, and a
mistake reads `language ini: state string, rule 2: unknown kind "strng"`.

## How oui_run's host is built

oui_run is three files and a script:

| File | What it does |
|------|--------------|
| [main.cpp](../examples/oui_run/main.cpp) | reads the command line, makes the `Application` and the `Host`, runs frames while the host keeps running |
| [quiet_backend.cpp](../examples/oui_run/quiet_backend.cpp) | the backend before a window: no screen, no input, sleeps until something wakes the loop |
| [host.cpp](../examples/oui_run/host.cpp) | `Host`: the engine, `SystemApi`, the native `__oui` calls, the widgets, the window |
| [prelude.js](../examples/oui_run/prelude.js) | the classes and functions scripts see as `oui`, built on `__oui` |

prelude.js is built into the program by `olagarro_embed_files` (as `embedded_oui_run_prelude()`), so oui_run is a
single file.

### Starting and ending

A script that only prints and runs commands is a terminal program like any other, so oui_run starts on
`QuietBackend`: the terminal is not taken over. `Host::open_window`, the first time a script calls `oui.window`,
switches the application to the terminal backend (or, with `--window` in the `oui_run_gui` build, a desktop window),
sets the theme unless the script chose one, and builds a frameless window of three `Part`s in a `DockLayout`: the menu
bar on top, the status bar at the bottom, the script's content between them.

`Host::run` runs the script as a module. Then `main` runs frames while `keeps_running()`: there is a window, or
`SystemApi::pending()` is above zero. `oui.quit()` ends it after the current work; `oui.exit(code)` throws
`ScriptStop`. While a window holds the terminal, what the script prints is kept and written once the terminal is given
back; an error is printed, and shown in a message box when there is a window.

The constructor sets the engine up in this order: time limit, print, unhandled rejections, `SystemApi`,
`define_everything()` for `__oui`, then prelude.js. Its members are declared so that the engine and `SystemApi` are
destroyed first: "what scripts hold dies before the widgets they reach, and the work they started before the engine its
callbacks run in."

### Native calls and JavaScript classes

`Host::define(name, ...)` defines `__oui.<name>`. These calls are plain and numeric: a widget is an integer id, and
each call does one thing, as `setChecked(id, value)`. prelude.js takes `__oui` into a local `n`, deletes it from the
global object, and builds the classes on it inside a block, so nothing it defines leaks into the script's scope:

```js
class CheckButton extends Widget {
    constructor(text, options = {}) {
        super(n.checkButton(String(text)));
        if (options.checked) this.checked = true;
        if (options.onToggle) this.onToggle = options.onToggle;
    }
    get checked() { return n.getChecked(this.id); }
    set checked(value) { n.setChecked(this.id, !!value); }
    set onToggle(fn) { n.on(this.id, "toggle", fn); }
}
```

The split puts each concern where it is easiest: argument conversion, defaults, options objects and getters and
setters in JavaScript; touching OUI's widgets in C++. At its end prelude.js freezes `globalThis.oui`, the system's calls
spread into it beside its own.

What C++ cannot ask a widget for, the class remembers: `TextEdit` and `Table` keep a private `#told` with the options
set on them, so their getters answer.

### Widget ids: adopt, take, widget

- `adopt(std::unique_ptr<Kind>)` gives a new widget the next id, records it in `m_widgets` (id to pointer) and keeps
  it in `m_loose` (made, not placed anywhere), which owns it.
- `take(id)` hands a loose widget over to whatever it goes in: a box, a grid, a tab, the window. A widget goes in one
  place only: taking it twice says "that widget is in place already".
- `widget<Kind>(id, "what")` finds a widget and checks its type with `dynamic_cast`; a wrong one throws
  `ScriptError{"not a " + what}`, which the script sees as a `TypeError`.
- `remove` takes a placed widget out of its parent and back into `m_loose`.

A description built with `oui.ui` gets its widgets numbered too: `Host::describe` adopts the root and records every
widget with an id, and returns their numbers and kinds (from `kind_of`, a chain of `dynamic_cast`s). prelude.js's
`existing(kind, id)` makes an object of the right class for each without making the widget again.

### Events through on()

Every event a script can hear goes through one native call, `__oui.on(id, event, fn)`, and `Host::on`:

- The function is stored in `m_handlers` under `(id, event)`. Only the first time is the widget's C++ signal
  connected, to a handler that looks the function up each time: setting `onClick` again replaces the function without
  stacking connections.
- The event name, and the widget's type where a name serves several, choose the signal: `"select"` is `on_selected`
  on a `ComboBox`, `on_selection_changed` on a `TabView` or a `Table`, and a path on a `TreeView`.
- Arguments go to the script as `Value`s; a function that throws is reported through `Host::call`.
- Where a handler must not run inside layout (a canvas's resize), it is posted.

`onKey` and context menus have native calls of their own (`onKey`, `contextMenu`), as they are not signals:
`set_key_handler`, and a right click seen by the application's event observer.

### Dialogs

`new oui.Dialog(title, content)` makes a `Window` and adopts it. `show()` calls `dialogShow`, which returns a promise
and posts the modal loop (`Application::show_modal_and_keep`), for the reason `later` gives: the script's call returns
first. When the dialog closes, it goes back to `m_loose`, whole, to be shown again, and the promise resolves with
whether it was accepted; the value `close(value)` gave is kept on the JavaScript side.

### Settings

`oui.settings.get/set/remove` keep values as JSON text, a key per line, in `<script>.ini` beside oui_run's own settings
in the user's configuration folder (`~/.config/oui_run` on Linux), saved as each is set.

## The api comments and the generated reference

Every name a script can use has a comment beside the code that defines it:

```cpp
// api(system): oui.readFile(path: string): string | null
// The file's text, or null when it cannot be read.
```

The first line says which scripts have it and declares it as TypeScript writes it; the comment lines right under it
describe it. A blank line or any other line ends the entry.

| Where | Which scripts |
|-------|---------------|
| `everywhere` | every script (`console.log`) |
| `system` | oui_run, and OUI Code's Run Script and Interactive Script (`SystemApi`) |
| `oui_run` | oui_run alone (prelude.js) |
| `oui_code` | OUI Code's language and command scripts (`oui.languages.register`, `oui.commands.register`) |
| `colouring` | both of those (`oui.words`) |

The declaration is one of:

| Form | Example |
|------|---------|
| a function | `oui.readFile(path: string): string \| null` |
| a value | `oui.args: string[]` |
| a class | `class oui.Label extends oui.Widget` |
| a constructor | `new oui.Label(text?: string, options?: {bold?: boolean, wrap?: boolean})` |
| a member | `oui.Label#text: string`, `oui.TextEdit#append(text: string): void` |

A class must be declared before its constructor and members. The same name may be declared twice, for two forms of a
function, as `oui.window(title, content)` and `oui.window(ui)`.

[tools/extract_api.js](../tools/extract_api.js) reads every `.cpp`, `.h` and `.js` under `oui/` and `examples/`, and
writes two files from the comments:

- `oui/oui.d.ts`: TypeScript declarations, a namespace per dotted name, for editors. OUI Code embeds it and reads it
  (`ScriptApi` in [completion.cpp](../examples/oui_code/completion.cpp)) for its suggestions and the signature shown
  while one writes a call.
- `doc/oui_api.md`: the reference, every name under its namespace, with which scripts have it.

```sh
bin/oui_run tools/extract_api.js            # writes both, from the repository's top
bin/oui_run tools/extract_api.js --check    # writes nothing; fails when they are out of date
```

`--check` also compares the declarations with oui_run's live `oui` object: a name on `oui` without a comment, or a
comment declaring a top-level `oui.x` that `oui` lacks, fails. The CTest test `Olagarro.OuiScriptApi` runs it, so a
change that forgets a comment, or forgets to regenerate, fails the build's tests.

## Adding a call or a widget to oui_run

The steps, with `CheckButton` as the model, as it has a constructor, a property and an event.

**1. Decide where it goes.** A call every scripting program should have (a file operation, a question) goes in
`SystemApi`: a `define` in `define_everything`, a wrapper in its prelude, and, when it writes, runs or asks, its name in
the `read_only` list. A call only oui_run has, which is anything about widgets and windows, goes in `Host`. The rest of
these steps are for `Host`.

**2. Define the native calls** in `Host::define_everything`, one small call per operation, ids as numbers:

```cpp
define("checkButton",
       [this](const Arguments& a) { return m_engine.number(adopt(oui::check_button(text_of(argument(a, 0))))); });
define("getChecked", [this](const Arguments& a)
       { return m_engine.boolean(widget<oui::CheckButton>(id_of(a, 0), "check button").is_checked()); });
define("setChecked",
       [this](const Arguments& a)
       {
           widget<oui::CheckButton>(id_of(a, 0), "check button").set_checked(argument(a, 1).to_bool());
           return Value{};
       });
```

Use `argument(a, n)` rather than `a[n]` (a missing argument is `undefined`), `text_of` for strings (`null` and
`undefined` become empty), and throw `ScriptError` with a message a script writer understands. A call that waits on
the user returns a promise and posts its work, as `dialogShow` does.

**3. Wire the events** in `Host::on`: a branch for the event name, and the widget's type when the name is shared,
connecting the C++ signal to `handler` with the arguments as `Value`s:

```cpp
else if(event == "toggle")
{
    widget<oui::CheckButton>(id, "check button")
        .on_toggled([this, handler](bool checked) { handler({m_engine.boolean(checked)}); });
}
```

**4. Write the class or function in prelude.js**, inside its block, converting arguments (`String`, `Number`, `!!`)
before they reach C++; then add it to the frozen `oui` object at the end. A widget that a description can make also
goes in `kinds` there, and gets its name in `kind_of` in host.cpp, so `ui.someId` is an object of its class.

**5. Write the api comments** at the top of prelude.js, with the others: the class, its constructor, each member. The
text is what the reference and OUI Code's signature tips show, so write it for the script's author.

```js
// api(oui_run): class oui.CheckButton extends oui.Widget
// A box to tick, with its text beside it.
// api(oui_run): new oui.CheckButton(text: string, options?: {checked?: boolean, onToggle?: (checked: boolean) => void})
// api(oui_run): oui.CheckButton#checked: boolean
// Whether it is ticked.
// api(oui_run): oui.CheckButton#onToggle: (checked: boolean) => void
// Called when it is ticked or unticked.
```

**6. Test it** in [tests/oui_run/run_tests.js](../tests/oui_run/run_tests.js): `runScript` writes a script in a
temporary folder, runs the oui_run under test on it, and gives what it printed and its exit code. Widgets work without
a window, so most tests build them and print what they hold:

```js
test("a check button is ticked from code, and says so", async () => {
    const {code, output} = await runScript(
        `const box = new oui.CheckButton("Loud", {checked: true});\n` +
        `box.checked = !box.checked;\n` +
        `console.log(box.checked);\n`);
    expect("code", code, 0);
    expect("output", output, "false\n");
});
```

`inTerminal(script)` runs a script with its window in a pseudo-terminal 40 columns by 10 rows and gives what it wrote,
escape sequences included, for what only shows on screen. Run the suite with `bin/oui_run
tests/oui_run/run_tests.js bin/oui_run`, and add a second argument to run the cases whose name contains it.

**7. Regenerate** `oui/oui.d.ts` and `doc/oui_api.md` with `bin/oui_run tools/extract_api.js`, and commit them with the
change. If the host shows a new text to the user through `translate`, add it to
`examples/oui_run/translations/es.json`: a test checks that every catalog covers every text of the sources (see
[translations](translations.md)). Then build and run `ctest`, which includes `Olagarro.OuiScriptApi` and
`Olagarro.OuiRunScripts`; see [testing](testing.md).
