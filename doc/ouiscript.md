# OUIScript for C++ developers

OUIScript runs JavaScript inside an OUI program, through QuickJS-ng. This document is for the C++ side: how to run
scripts, give them functions to call, hand values back and forth, and keep a script from hanging the program; the calls
every scripting program shares; and the highlighters scripts define. Writing scripts is the subject of [scripting with
oui_run](https://github.com/iganinja/oui_code/blob/master/doc/scripting.md), and every name a script can use is in [the
API reference](https://github.com/iganinja/oui_code/blob/master/doc/oui_api.md), both in [OUI Code's
repository](https://github.com/iganinja/oui_code).

- [The pieces](#the-pieces)
- [ScriptEngine](#scriptengine)
- [Values](#values)
- [Promises settled from C++](#promises-settled-from-c)
- [Time limits](#time-limits)
- [Errors](#errors)
- [Unhandled rejections](#unhandled-rejections)
- [SystemApi: the calls every program shares](#systemapi-the-calls-every-program-shares)
- [Highlighting rules](#highlighting-rules)
- [oui_run, the api comments and the API reference](#oui_run-the-api-comments-and-the-api-reference)

## The pieces

| Header | What it is |
|--------|------------|
| [script_engine.h](../oui/include/oui/script/script_engine.h) | `ScriptEngine`, `Value`, `ScriptError`, `ScriptStop` |
| [system_api.h](../oui/include/oui/script/system_api.h) | `SystemApi`: files, programs, timers, questions, clipboard |
| [rule_highlighter.h](../oui/include/oui/script/rule_highlighter.h) | `RuleHighlighter`: colouring by states of regular expression rules |
| [script_highlighter.h](../oui/include/oui/script/script_highlighter.h) | `highlighter_of`: a `RuleHighlighter` from a script's states |

They build into the library `olagarro_oui_script` (CMake target `olagarro::OUIScript`, the component `OUIScript` of an
installed package), which links OUI publicly and QuickJS privately: nothing of QuickJS shows in the headers. The sources
are in `oui/src/script`.

The programs in [OUI Code's repository](https://github.com/iganinja/oui_code) host scripts in three ways, each a
working example of a different use:

- oui_run: a script is the whole program. `Host`
  ([host.cpp](https://github.com/iganinja/oui_code/blob/master/oui_run/host.cpp)) gives it `oui`, built from native
  calls and [prelude.js](https://github.com/iganinja/oui_code/blob/master/oui_run/prelude.js).
- OUI Code's language and command scripts: `Scripting`
  ([scripting.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/scripting.cpp)) runs them once at start and
  keeps the functions they register to call later.
- OUI Code's Run Script and Interactive Script: `ScriptRun`
  ([script_run.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/script_run.cpp)) and `LiveScript`
  ([live_script.cpp](https://github.com/iganinja/oui_code/blob/master/oui_code/live_script.cpp)) load a module, call the
  function it exports, and wait for what it started. See [OUI Code's
  architecture](https://github.com/iganinja/oui_code/blob/master/doc/oui_code.md#scripting).

## ScriptEngine

A `ScriptEngine` is one QuickJS runtime and context. It is used by one thread at a time: the one that made it, or
another it was handed to, as a renderer's engine is passed between the threads of a task pool, the program making sure
two never use it at once. Each run, call or read from outside a script measures the stack from the thread making it.
Work that ends elsewhere, as a process's exit, comes back through `Application::post`. Scripts run in strict mode.

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
// OUI Code's script_run.cpp's prelude, all a Run Script sees
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

## oui_run, the api comments and the API reference

How oui_run builds its `oui` object on `ScriptEngine` and `SystemApi`, the comments marked `api` beside every name a
script can use, from which the API reference is made, and how to add a call or a widget to oui_run are in [oui_run's
host](https://github.com/iganinja/oui_code/blob/master/doc/oui_run.md), in OUI Code's repository. The comments for what
OUI defines, as `SystemApi`'s calls, are in OUI's sources, here.
