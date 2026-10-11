# Scripting with oui_run

oui_run runs JavaScript files as programs, the way node does, with OUI's own calls under `oui`: files, commands,
questions to the user, and windows made of OUI's widgets. The same scripts, as far as they use files, commands and
questions, run inside OUI Code too, on the document or on the project.

This guide is for someone who knows JavaScript and wants to get something done: what there is, how each thing is
done, and where to copy from. [The API reference](oui_api.md) lists every name with its exact signature;
[`examples/oui_run/recipes`](../examples/oui_run/recipes) has whole scripts for common jobs.

- [Running a script](#running-a-script)
- [What is there, coming from node](#what-is-there-coming-from-node)
- [Arguments](#arguments)
- [Files and folders](#files-and-folders)
- [Running commands](#running-commands)
- [Asking the user](#asking-the-user)
- [Settings](#settings)
- [The clipboard and the system](#the-clipboard-and-the-system)
- [Timers](#timers)
- [Modules](#modules)
- [Ending, and errors](#ending-and-errors)
- [Windows](#windows)
- [Scripts in OUI Code](#scripts-in-oui-code)
- [Recipes](#recipes)
- [Programs](#programs)
- [Types for your editor](#types-for-your-editor)

## Running a script

```sh
oui_run script.js [arguments...]
oui_run_gui --window script.js [arguments...]   # a window on the desktop, where the build has one
```

A script is a JavaScript module: `import` works, and so does `await` at the top level. The program ends when the
script has run and everything it started is done: timers, commands, an open window.

```js
// hello.js: oui_run hello.js Ana
const name = oui.args[0] ?? "world";
console.log(`Hello, ${name}.`);
const code = await oui.run("uname -sr");      // its output goes to the terminal
console.log(`uname ended with ${code}.`);
```

## What is there, coming from node

All of modern JavaScript (the engine is QuickJS-ng): classes, `async`/`await`, modules, `Map` and `Set`, regular
expressions with named groups and Unicode properties, `JSON`; not `Intl`. What node adds is not there either: no
`require`, `process`, `fs`, `path`, `Buffer` or `fetch`. Instead:

| node | oui_run |
|------|---------|
| `process.argv.slice(2)` | `oui.args` |
| `process.exit(code)` | `oui.exit(code)` |
| `fs.readFileSync(path, "utf8")` | `oui.readFile(path)`, `null` when it cannot |
| `fs.writeFileSync(path, text)` | `oui.writeFile(path, text)`, `false` when it cannot |
| `fs.existsSync(path)` | `oui.exists(path)` |
| `fs.readdirSync(path)` | `oui.listDir(path)`, folders ending in `/` |
| `child_process.exec(command)` | `await oui.run(command, {output})` |
| `fs.renameSync`, `mkdirSync`, `rmSync` | a command: `await oui.run("mkdir -p out")` |
| `readline` questions | `await oui.prompt(...)`, `oui.confirm`, `oui.choose` |
| `fetch(url)` | a command: `curl` with `oui.run` |

Files are text, read and written as UTF-8. Paths are relative to the folder oui_run was started in. A path OUI gives
a script, as OUI Code's to a script it runs, has `/` between folders on every system, Windows too; `oui.args` are as
they were typed. Windows takes a path written either way; only a command for `oui.run` may want `\` there, as some
Windows commands take `/` for an option.

## Arguments

`oui.args` has what came after the script's name, as strings. A few lines are enough to take options apart; the
recipes share this one, in `lib/files.js`:

```js
// "--name" and "--name=value" as options, the rest in order.
function parseArgs(args) {
    const options = {};
    const positional = [];
    for (const argument of args) {
        const option = /^--([\w-]+)(?:=(.*))?$/.exec(argument);
        if (option) options[option[1]] = option[2] ?? true;
        else positional.push(argument);
    }
    return {options, positional};
}

const {options, positional: [pattern, folder = "."]} = parseArgs(oui.args);
if (!pattern) {
    console.log("usage: oui_run search.js PATTERN [FOLDER] [--ignore-case]");
    oui.exit(2);
}
```

## Files and folders

```js
const text = oui.readFile("notes.txt");           // a string, or null when it cannot be read
if (text === null) { console.log("no notes"); oui.exit(1); }

oui.writeFile("notes.txt", text.toUpperCase());   // true, or false when it cannot be written
oui.exists("build");                              // a file or a folder
oui.listDir("src");                               // ["lib/", "main.js", "util.js"]: in order, folders end in "/"
```

`listDir` gives an empty list for a folder that is not there, and for a file. Going through a whole tree is a short
recursive function:

```js
function walk(folder, found = []) {
    for (const name of oui.listDir(folder)) {
        if (!name.endsWith("/")) found.push(`${folder}/${name}`);
        else if (name !== ".git/") walk(`${folder}/${name.slice(0, -1)}`, found);
    }
    return found;
}

for (const file of walk("src").filter((file) => file.endsWith(".js"))) { ... }
```

`readFile` reads any file, a binary one too, as text that is not: a NUL character (`"\0"`) in it is the usual sign
to leave it alone. A text's `length` counts UTF-16 units; `[...text].length` counts characters.

There is no call to make, move or delete files or folders: a command does it, as for anything else `oui` does not
have. See the next section, and quote what goes into a command.

## Running commands

`oui.run(command, options)` runs a command through the system's shell (`/bin/sh` on Linux and macOS, `cmd` on
Windows) and gives a promise of its exit code.

```js
// Its output straight to the terminal, as it comes.
const code = await oui.run("make -j8");

// Its output to the script instead: in pieces as it comes, which need not end at a line, so keep them all.
let printed = "";
const status = await oui.run("git status --porcelain", {output: (text) => { printed += text; }});
const changed = printed.split("\n").filter(Boolean).map((line) => line.slice(3));

// What it reads, and where it runs.
await oui.run("sort -u", {input: "b\na\nb\n", cwd: "/tmp", output: (text) => console.log(text)});
```

`output` gets what the command writes to standard output and to standard error, together. A command that is not
there runs like any other, as the shell does: it ends with 127 and says why, so check the code.

The promise also has `stop()`, to end the command before it ends by itself:

```js
const build = oui.run("make");
const timer = setTimeout(() => build.stop(), 60_000);
const code = await build;
clearTimeout(timer);
```

Anything taken from a file name or the user goes into a command quoted, or a space, a quote or a `;` in it does
something else. For a POSIX shell:

```js
const quote = (text) => `'${text.replaceAll("'", "'\\''")}'`;
await oui.run(`mv -- ${quote(from)} ${quote(to)}`);
```

## Asking the user

These calls ask and wait, each a promise. With a window open they are dialogs; without one, questions on the
terminal.

```js
await oui.message("Done", "Every file is tidy.");
if (await oui.confirm("Replace 12 matches?")) { ... }            // true or false
const name = await oui.prompt("New file", "Name:", "notes.txt");  // the text, or null when cancelled
const pick = await oui.choose("Build", ["debug", {label: "release", detail: "optimised, no asserts"}]);
                                                                  // a label, or null
const file = await oui.openFile({filters: [{name: "Texts", patterns: "*.txt;*.md"}]});   // a path, or null
const where = await oui.saveFile({name: "report.csv"});           // asks before replacing one
const folder = await oui.chooseFolder();
```

On the terminal, `confirm` takes y or n, `prompt` a line, `choose` the option's number, and the file and folder
ones a path. When input is not a
terminal, as with `< answers.txt` or a pipe, they read their answers from it.

## Settings

`oui.settings` keeps what a script wants to find again the next time it runs, by names of its choosing, any value JSON
holds. It is saved as it is set, in `<script>.ini` in the user's configuration folder for oui_run,
`~/.config/oui_run` on Linux, so scripts of the same name share it.

```js
const size = oui.settings.get("size", [80, 24]);  // what was set, or the fallback
oui.settings.set("size", [100, 30]);
oui.settings.remove("size");
```

## The clipboard and the system

```js
oui.copy("text");                                 // to the clipboard, the desktop's when there is one
const text = oui.paste();                         // what is on it
oui.openExternally("https://example.org");         // a link or a file, in what the system opens it with
```

## Timers

`setTimeout`, `setInterval`, `clearTimeout` and `clearInterval`, as in a browser. A timer keeps the program alive
until it fires or is cleared. A pause is a promise:

```js
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Every second, until the file changes.
const before = oui.readFile("out.log");
while (oui.readFile("out.log") === before) {
    await sleep(1000);
}
```

## Modules

A script imports others with paths relative to itself, and exports as any module does:

```js
// lib/csv.js
export function parseCsv(text) { ... }

// report.js
import {parseCsv} from "./lib/csv.js";
const {options} = await import("./options.js");   // import() too, when it is needed
```

A module runs once, when it is first imported. One that is to be imported should only define things: leave what
the program does to the script that is run.

## Ending, and errors

- The program ends when nothing is left to do, with 0.
- `oui.exit(code)` ends it there and then: nothing after it runs, not even a `catch` or a `finally`.
- `oui.quit()` ends it once what it is doing now is done, as closing the window does.
- An error nobody catches, or a promise rejected that nobody handles, is written with where it happened, and the
  program ends with 1:

```text
oui_run: Error: boom
    at <anonymous> (/home/ana/report.js:6:11)
```

## Windows

`oui.window(title, content)` opens a window: in the terminal as a text interface, or on the desktop with
`oui_run_gui --window`. The content is widgets, made with `new`, laid out with `oui.column`, `oui.row` and
`oui.split`, and wired together by their callbacks. The window keeps the program alive until it closes, with
Ctrl+Q or the window's close button, or with `oui.quit()`.

```js
const output = new oui.TextEdit({readOnly: true});
const field = new oui.LineEdit({
    placeholder: "Command",
    onSubmit: async (command) => {
        output.append(`$ ${command}\n`);
        const code = await oui.run(command, {output: (text) => output.append(text)});
        output.append(`[exit ${code}]\n`);
        field.text = "";
    },
});

oui.shortcut("Ctrl+L", () => { output.text = ""; });
oui.window("Runner", oui.column([field, oui.fill(output)]));
field.focus();
```

| Widget | Made with | What it has |
|--------|-----------|-------------|
| `oui.Label` | `new oui.Label(text, {bold, wrap})` | `text` |
| `oui.Button` | `new oui.Button(text, onClick, {default})`; the default one is what Enter presses in a dialog | `text`, `onClick` |
| `oui.LineEdit` | `new oui.LineEdit({text, placeholder, onChange, onSubmit})` | `text`, `placeholder`, `selection`, `select(start, end)`, `selectAll()`, `selectedText`, `insert(text)`, `onChange`, `onSubmit` |
| `oui.TextEdit` | `new oui.TextEdit({text, readOnly, wrap, lineNumbers, onChange})` | `text`, `readOnly`, `wrap`, `lineNumbers`, `append(text)`, `onChange`, and an editor's: see below |
| `oui.CheckButton` | `new oui.CheckButton(text, {checked, onToggle})` | `checked`, `onToggle` |
| `oui.ComboBox` | `new oui.ComboBox(items, {onSelect})` | `items`, `selected`, `onSelect` |
| `oui.List` | `new oui.List(items, {checks, onSelect, onActivate, onToggle})` | `items`, `selected`, `checked`, `onSelect`, `onActivate`, `onToggle` |
| `oui.Table` | `new oui.Table(headings, {widths, rows, onSelect, onActivate, onSort})` | `rows`, `selected`, `sortBy(column, order)`, `widths`, `stretch`, `resizable`, `headings`, `show(row)`, `onSelect`, `onActivate`, `onSort`, `onResize` |
| `oui.Tree` | `new oui.Tree(items, {onSelect, onActivate})`, items as `{text, children, expanded}` | `items`, `selected` (a path, `[1, 0]`), `onSelect`, `onActivate` |
| `oui.MarkdownView` | `new oui.MarkdownView(markdown, {onLink})` | `markdown`, `goTo(section)`, `onLink` with where a link that leaves the text leads |
| `oui.Tabs` | `new oui.Tabs({fixed, onSelect, onClose})`, then `add(title, widget)` | `count`, `pages`, `selected`, `title(index)`, `setTitle(index, title)`, `remove(index)`, `move(from, to)`, `marked`, `onSelect`, `onClose` |
| `oui.Split` | `new oui.Split(a, b, {vertical, ratio})`, or `oui.split` | `ratio` |
| `oui.Box` | `oui.column(widgets, {gap})`, `oui.row(widgets, {gap})` | `add(widget, {fill})` |
| `oui.Grid` | `new oui.Grid({columns, rows, gap})`, lengths as a description's | `add(...widgets)`, filled a row at a time |
| `oui.Dock` | `new oui.Dock()` | `add(side, widget, {size, resizable})`, side `"top"`, `"bottom"`, `"left"`, `"right"` or `"fill"` |
| `oui.Panel` | `new oui.Panel(widget, {padding})` | what it holds, with room round it, on the panels' colour |
| `oui.ScrollView` | `new oui.ScrollView(widget)` | what it holds, scrolled when it is taller than its room |
| `oui.HSpacer`, `oui.VSpacer` | `new oui.HSpacer(cells)`, `new oui.VSpacer(rows)` | room, and nothing else |
| `oui.Comparison` | `new oui.Comparison({left, right, leftTitle, rightTitle})` | `compare(left, right, leftTitle, rightTitle)`, `differences`, `next()`, `previous()` |
| `oui.FolderView` | `new oui.FolderView(folder, {onActivate})` | `folder`, `selected`, `onActivate` with a file's path |
| `oui.Terminal` | `new oui.Terminal({command, folder, onFinished, onTitle})`, the user's shell unless a command is given | `paste(text)`, `running`, `onFinished`, `onTitle` |
| `oui.Calculator` | `new oui.Calculator({onExit})` | what oui_calc is, in a widget |
| `oui.Canvas` | `new oui.Canvas({width, height, onMouse, onResize})` | `width`, `height`, `clear(background)`, `text(x, y, text, style)`, `fill(x, y, width, height, background)`, `onMouse`, `onResize` |

Every widget has:

- `focus()`, which gives it the keyboard; a column or a calculator gives it to the first of its widgets that takes it;
- `visible` and `enabled`: a disabled one shows so and answers nothing;
- `tooltip`, what shows by the pointer resting on it;
- `background` and `color`, colours of the theme by name, as `"table_background"`, or fixed ones, as `"#20a0ff"`;
- `contextMenu`, entries as a menu's that a right click on it opens;
- `onKey(key, text)`, which hears the keys before the widget does, `"Ctrl+S"` or `"Enter"` and what they type, and
  keeps one from going further by returning true;
- `remove()`, which takes it out of what it is in, to `add` it somewhere else or nowhere.

Layout:

- `oui.column([a, b, oui.fill(c)])`: one under the other; the one in `oui.fill` takes the room left.
- `oui.row([a, oui.fill(b), c])`: side by side, the same way. Both take `{gap: "none" | "tight" | "normal" |
  "loose"}`.
- `oui.split(a, b, {vertical, ratio})`: two parts with a line between them that the mouse drags.
- `new oui.Tabs().add("One", a).add("Two", b)`: pages one at a time, under their tabs.
- `new oui.Grid({columns: [10, "fill"]}).add(label, field, ...)`: in columns, a row at a time, as a form.
- `new oui.Dock().add("top", bar).add("left", tree, {resizable: 12}).add("fill", body)`: against the edges, the last
  in what they leave.

A text edit is an editor, as OUI Code's. Places in its text are as JavaScript counts them in a string:

```js
edit.find("TODO", {matchCase: false});            // selects the next one; true when there was one
edit.replaceAll("colour", "color");               // how many, as one step to undo
edit.select(0, 5); edit.insert("Hello");          // as typing would
edit.goToLine(12); edit.undo();
console.log(edit.line, edit.column, edit.selectedText, edit.modified);
edit.highlight = {start: [{match: oui.words(["if", "else"]), kind: "keyword"}, {match: /\/\/.*/, kind: "comment"}]};
```

`highlight` takes the states an OUI Code language colours with, the same rules. Also there: `duplicateLines()`,
`moveLines("up")`, `deleteLines()`, `toggleComment("//")`, `showMatches(text)`, `tabSpaces`, `useSpaces`,
`autoIndent`, `occurrences` and `onCursor`.

A canvas is drawn on in cells, as text and filled areas, which stay until it is cleared. `onResize` is where to draw
it, the first time too:

```js
const board = new oui.Canvas({
    onResize: (width, height) => board.clear().fill(0, 0, width, 1, "#0000aa").text(1, 0, "Score: 0", {bold: true}),
    onMouse: (mouse) => { if (mouse.kind === "press") oui.status(`${mouse.x}, ${mouse.y}`); },
});
```

The window can have menus along its top and a status line along its bottom, before or after it opens:

```js
oui.menuBar([{title: "&File", items: [{title: "&Open...", keys: "Ctrl+O", run: openFile}, "-",
                                      {title: "&Quit", keys: "Ctrl+Q", run: () => oui.quit()}]}]);
oui.status("Ready", "12 files");                                 // the text, and what goes at its right end
```

`oui.theme(name)` changes the look: `"turbo"`, `"night"`, `"light"`, `"monochrome"`, `"amber"`, `"phosphor"` or
`"commander"`, and `"classic"` in a desktop
window. On the desktop the window opens at the size of what it holds, and its width is a description's `width=` when it
has one.

Callbacks can be `async`, and can ask with `oui.confirm` and the rest, which open as dialogs over the window.

A dialog of the script's own is a widget, as a column of others, under a title, over the window. `show()` opens it
and waits: what `close(value)` was given, or null when Escape or its close button cancelled it. It keeps its widgets
as they were left, to open again.

```js
const name = new oui.LineEdit({onSubmit: (text) => dialog.close(text)});
const ok = new oui.Button("OK", () => dialog.close(name.text), {default: true});
const dialog = new oui.Dialog("Rename", oui.column([new oui.Label("New name:"), name, ok]), {width: 40});
const chosen = await dialog.show();               // the name, or null
```

A field keeps Enter for itself, so the dialog above closes from its `onSubmit` too; from any other widget, Enter presses
the default button.

A description whose root is a `Window` is a dialog as it is a window: `new oui.Dialog(ui)` takes its title, its size and
what it holds from the `.oui` file, so the same file serves either way.

```js
const ui = oui.uiFromFile("ui/rename.oui");      // Window "Rename" width=40, a LineEdit id=name, a Button id=ok
const dialog = new oui.Dialog(ui);
ui.ok.onClick = () => dialog.close(ui.name.text);
```
`examples/oui_run/demo.js` is a fuller panel, and the recipe `search_window.js` a search with a list and a preview.

### Described in a .oui file

A window can also be written down rather than built: what there is and where in a `.oui` file, as OUI Code's own
dialogs are, and what it does in the script. `oui.uiFromFile(path)` reads one, found from the script's folder
wherever it is run from, and `oui.ui(text)` takes one in a string. Each widget with an `id` is there by it, as the
object of its kind, with what the table above gives it.

```
# ui/ask.oui
Window "Greet"
  Grid columns=content,1,fill
    Label "&Name:" buddy=name
    HSpacer
    LineEdit id=name placeholder="Who to greet"
  Label id=greeting
  Row centered gap=tight
    Button "&Greet" id=go
    Button "Close" id=close
```

```js
const ui = oui.uiFromFile("ui/ask.oui");
ui.go.onClick = () => { ui.greeting.text = `Hello, ${ui.name.text}!`; };
ui.close.onClick = () => oui.quit();
oui.window(ui);
ui.name.focus();
```

A line is a widget, its text in quotes after its type, then its properties; what goes in it is indented under it. `#`
starts a comment. `&` marks the letter that, with Alt, presses a button or goes to the field a label names. The
widgets:

| Widget | Properties |
|--------|------------|
| `Window "title"` | its root: `oui.window(ui)` opens it, titled so, with what it holds |
| `Column`, `Row` | `gap=none\|tight\|normal\|loose`; a row's `centered` |
| `Grid` | `columns=content,fill,12` and `rows=content,flex` (each `content`, `fill`, `flex` or a number of cells), `gap`; its widgets fill it a row at a time |
| `Split` | `vertical`, `ratio=0.4`; its two widgets with a line between them |
| `Tabs` | `fixed`; `Tab "title"` in it, each holding a widget or none |
| `Label "text"` | `buddy=id`, the field its `&` letter goes to (with none the `&` does not show; `&&` is one), `bold`, `heading`, `wrap` |
| `LineEdit` | `text="..."`, `placeholder="..."` |
| `TextEdit` | `read_only`, `wrap` |
| `CheckButton "text"` | `checked` |
| `Button "text"` | `default`, `compact`, `flat` |
| `ComboBox` | its items are the script's: `ui.size.items = [...]` |
| `VSpacer`, `HSpacer` | `height=rows`, `width=cells` |
| `Slot` | a place the script fills: `ui.put("results", new oui.List([]))` |

Every widget takes `id=name`, `hidden` and `tooltip="..."`. A Label, a LineEdit, a CheckButton and a MarkdownView take
`background=` with a colour of the theme by its name, as `table_background`, and a MarkdownView `foreground=` too; a
script sets them through `background` and `color`, which take fixed colours too. A description that is wrong stops the script there, saying the line and the column. The
recipe `rename_window.js` is a whole one. In OUI Code, **Tools > Interactive UI** shows what a description builds
as it is written, coloured, and where it is wrong.

## Scripts in OUI Code

OUI Code runs scripts on what you are editing. They have files, commands, questions, the clipboard and timers, as
here, but not `oui.args`, `oui.exit` or windows; a script is a module that exports what OUI Code calls:

```js
// Tools > Run Script, on the document: file is its whole path. F5 runs it as a dry run, on a copy;
// Shift+F5 for real.
export function processFile(file, dryRun) {
    const text = oui.readFile(file);
    console.log(`${text.split("\n").length} lines`);
    if (!dryRun) oui.writeFile(file, text.replace(/[ \t]+$/gm, ""));
}

// Tools > Run Script, on its own: folder is the project's.
export async function process(dryRun, folder) { ... }

// After Generate in an Interactive UI tab, chosen beside it for each language: what Generate wrote, to save or
// build with; what it gives back shows at the end of its output. In an Interactive Script tab, a dry run.
export async function processGeneratedCode(code, {language, file, dryRun}) { ... }

// Tools > Interactive Script: the document's text in, the text in its place out, shown as you write the script.
export function transform(text) {
    return text.split("\n").sort().join("\n");
}
```

`dryRun` is not optional: a dry run changes nothing and says what it would do. Interactive scripts only look: they read
files, but writing, running commands, asking, copying and opening do not work in them. OUI Code's help (F1) has the
rest, and how its own language and command scripts, `oui.languages.register` and `oui.commands.register`, are written.

## Recipes

Whole scripts in [`examples/oui_run/recipes`](../examples/oui_run/recipes), each with how to run it at the top,
tested on every build. Copy one and change it.

| Recipe | What it does | How |
|--------|--------------|-----|
| `count.js` | Lines, words and characters of files, as `wc` | Walking folders, counting characters, columns of text |
| `search.js` | A regular expression in every file, as `grep -rn` | Options, skipping binary files, the exit code |
| `replace.js` | Replaces across files, showing each line before and after | A dry run by default, `--write`, asking first |
| `csv_summary.js` | Adds up a CSV column grouped by another | A CSV parser with quotes, `Map`, sorting |
| `json_to_csv.js` | A JSON array of objects as a CSV | `JSON.parse`, filtering, sorting as people read |
| `tally.js` | Counts the lines a command prints | Taking a command's output in |
| `todo_report.js` | A Markdown report of TODO and FIXME comments | Writing a report, to a file or the terminal |
| `rename.js` | Renames files by a regular expression | A command for what `oui` lacks, quoting, checks first |
| `search_window.js` | `search.js` in a window, with a preview | Widgets, layout, callbacks, shortcuts |
| `rename_window.js` | `rename.js` in a window, each new name shown as it is typed | A window described in `ui/rename_window.oui`, a `Slot` |
| `lib/files.js`, `lib/csv.js`, `lib/rename.js` | What the others share | Modules |
| `oui_code/trim_trailing_spaces.js` | For Run Script: tidies the document | `processFile(file, dryRun)` |
| `oui_code/check_links.js` | For Run Script: Markdown links that lead nowhere | `process(dryRun, folder)` |
| `oui_code/markdown_toc.js` | For Interactive Script: a table of contents | `transform(text)` |

## Programs

Small programs in [`examples/oui_run/programs`](../examples/oui_run/programs), in a window of their own, each with how
to run it at the top and tested on every build.

| Program | What it is | How |
|---------|------------|-----|
| `2048.js` | The game: slide the tiles with the arrows | A description written as text, `oui.shortcut`, a seed for repeatable games |
| `processes.js` | The programs running, as `top` | A `Table` its headings order, a command's output read every two seconds with `setInterval`, a filter, asking before acting |
| `files.js` | Folders, and what is in their files beside them | `oui.listDir`, a `Split`, Markdown and code in a `MarkdownView` |
| `todo.js` | A list of things to do, kept as JSON | A window described in `ui/todo.oui`, a `Slot`, `JSON` read and written |

## Types for your editor

`oui/oui.d.ts` declares everything in `oui` as TypeScript, with what each does. OUI Code reads it by itself: it
suggests `oui.` names as you type and shows how each is called. Another editor, VS Code for one, reads it from a
comment at the top of the script:

```js
/// <reference path="../oui/oui.d.ts" />
```

or from a `jsconfig.json` beside the scripts, `{"compilerOptions": {"checkJs": true}, "include": ["*.js",
"../oui/oui.d.ts"]}`, with the paths as they are from there.
