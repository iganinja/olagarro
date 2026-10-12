# OUI Code

A text editor for the terminal, built on OUI. It runs the same locally and over SSH, and needs nothing
installed besides itself. Press **Esc** to close this help; **Left** and **Right**, or **Ctrl+Page Up** and
**Ctrl+Page Down**, go from one part to another, and **Page Up**, **Page Down**, **Home** and **End** move
through each.

## Getting around

- **F10** opens the menus, and **Alt** with a menu's underlined letter opens that one. Some terminals keep Alt
  and a letter for themselves; F10 always reaches OUI Code.
- **F1** opens this help.
- **Ctrl+L** in a window such as Open File places it: the arrows move it, Ctrl and the arrows resize it, Enter
  keeps it there and Esc puts it back. Windows open again where you left them; the mouse drags their title
  and their edges too.
- **Ctrl+Shift+P** opens the command palette: every command, with its keys. Type part of a name to narrow the
  list, then Enter. It is in the View menu too, for terminals that send Ctrl+Shift+P as Ctrl+P.
- **Ctrl+P** goes to a file of the project by typing part of its name (see *Finding files*).
- **Alt+←** goes back to where the cursor was before it jumped, to another document or ten lines or more away,
  however it went there: Go to Line, a search, a click, a file opened. **Alt+→** goes forward again. A document
  closed since opens again.
- **F4** moves the focus round the project tree, the editor and the explorer.
- **Ctrl+Page Down** and **Ctrl+Page Up** go to the next and the previous tab, round from the last to the first.
  **Shift+F4** lists the open tabs, to pick one with the arrows and Enter. In a window, **Ctrl+Tab** and
  **Ctrl+Shift+Tab** do the same as Ctrl+Page Down and Ctrl+Page Up.
- A tab shows the file's name; the status bar shows its whole path, and so does resting the pointer on the tab.
- **Ctrl+K** splits the editor: the bottom line lists the keys, and an arrow opens a new group of tabs on that
  side of the current one, taking the current tab when its group has others. Split again and again to lay out
  as many groups as the screen holds. After Ctrl+K, Shift and an arrow move the current tab to the group on
  that side instead, Alt and the arrows move the line beside the group as often as you like until Enter, and
  Backspace merges the group into the one beside, which takes its tabs. **Alt+K** moves round the window:
  each arrow goes to what is nearest on that side, another group, a part of the tab (the code and the result of
  an interactive script or user interface) or a side panel, and Page Up and Page Down go to the tab before or
  after; any other key ends it and does what it does, so you can start typing straight away. It works from a
  terminal tab too, which keeps every other key for its programs. A group left with no tabs closes. Commands act on the group with the focus; F4 goes
  round all of them, and the lines between them can be dragged.
- **F9** opens a preview of a Markdown document in a tab of its own, which follows the document as you write;
  F9 on the preview goes back to the document. Its links are followed with a click, or with **Tab** to one and
  **Enter**: another Markdown file is read in the preview, as in a browser, and **Alt+Left** and **Alt+Right** go
  back and forward; a section of one goes to its heading; a web address opens in the browser.
  **Ctrl+Shift+E** opens the file the preview shows to edit it.

- **Ctrl+T** opens a new tab, of what it is to hold: a new file, a terminal, the calculator, an interactive script
  or user interface, a comparison of two files, the task list or the code's TODOs.

## Projects and files

Opening a folder (**File > Open Folder**, a recent folder, or `oui_code folder`) makes it the *project*. The
**project tree** on the left shows it and never goes above it; **Ctrl+B** shows or hides it. The **explorer**
on the right (**Ctrl+E**) roams the whole file system, to bring things into the project. Their headers say which
is which, and the one with the focus stands out.

OUI Code comes back as it was left, tabs and all, however it starts. Files named when starting it,
`oui_code notes.txt todo.md`, open over that, the last in front; one not there yet opens empty under its name,
and saving it makes it.

Both panels take the same keys:

| Key | Action |
|-----|--------|
| Enter or a click | Open a file, or go into a folder |
| Backspace | Go up a folder |
| Typing | Jump to the entry whose name starts that way |
| Insert | New file, which opens in a tab |
| F7 | New folder |
| F2 | Rename |
| Delete | Delete, after asking |
| F5 | Copy to the folder the other panel shows |

A right click on an entry opens a menu with the same: new file or folder, rename, delete and copy. On a file it
also offers **Open As**, as text or as a task list, such as a task list's own Markdown to edit by hand; a file opens
as a task list only when it holds nothing else, so none of it is lost. The line on
the inner edge of either panel drags to make it wider or narrower, and it stays so.

The project, the panels, the explorer's folder and the editor's groups with their files come back as you left
them when OUI Code starts with nothing to open.

## Finding files

**Ctrl+P** lists the project's files. In a git repository it leaves out what git ignores, as its .gitignore
files say; elsewhere, hidden files and folders and build folders. What you type narrows them:

- Plain letters find names *fuzzily*: the letters in order, not necessarily together. `tblcpp` finds
  `table.cpp`. Letters that start words and fall in the file name count most.
- A pattern with `*` or `?` works as in a shell: `*.h` finds every header, `hola*.h` the headers whose name
  starts with "hola", `*edit*` any name with "edit" in it. With a `/` the pattern covers the whole path, as in
  `src/*.h`.

## Editing

| Keys | Action |
|------|--------|
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+C, Ctrl+X, Ctrl+V | Copy, cut, paste; also Ctrl+Insert, Shift+Delete, Shift+Insert |
| Ctrl+A | Select all |
| Ctrl+Left / Ctrl+Right | Move by words |
| Tab / Shift+Tab | Indent or outdent; on several lines, every line |
| Alt+Up / Alt+Down | Move the line up or down |
| Ctrl+Shift+D | Duplicate the line |
| Ctrl+Shift+K | Delete the line |
| Ctrl+/ | Comment or uncomment the lines; Ctrl+7 on a Spanish keyboard |
| Ctrl+Shift+M | Go to the bracket that pairs with the one beside the cursor |
| Shift+Alt+Up / Shift+Alt+Down | Add a cursor on the line above or below |

With several cursors, typing, deleting, moving, copying and pasting happen at all of them. Pasting as many lines
as there are cursors gives each cursor its line. Escape or a click leaves one cursor.

The bracket beside the cursor and its pair stand out, whatever they are inside: OUI Code does not yet know
strings and comments apart from code.

Selecting text within a line marks the other places in the document with the same text, letter for letter, as
where else a name is used.

As you write, OUI Code suggests how the word may go on, once it has two letters: the words of the open documents,
what the document's language offers, as its keywords and pieces of text for a loop or a function, and in a script
(`.js`) what OUI's scripts can use, after `oui.` with how each is called, and after a name made with
`new oui.Label(...)` or another of its classes, what that has. Up and Down pick, Enter or Tab put it in
place of the word, Escape closes the list, and writing on narrows it. **Ctrl+Space**, or **Alt+/** in a terminal
that keeps Ctrl+Space, asks for suggestions at any time, and says so when there is nothing to suggest. Preferences
turn off the list that opens by itself.

In a script, inside the brackets of a call to what OUI's scripts can use, as `oui.writeFile(`, a note over the
line shows how it is called, with the parameter you are writing marked, and what it does. It follows the cursor
from one argument to the next and goes when the brackets close; Escape closes it until you are in another call.

## Searching

- **Ctrl+F** opens the find bar, with what you looked for last selected so typing replaces it. Matches light up as
  you type and it counts them ("3 of 12"), and the first one from the cursor is selected and shown. Enter goes to the
  next one and into the document there; **F3** and the arrows beside the bar move between them. Its options: whole
  word, regular expression, match case.
- **Ctrl+F3** looks for what is selected, or the word at the cursor when nothing is, and goes to the next place
  with it, without opening the bar; then **F3** and **Shift+F3** go from one to the next.
- **Ctrl+H** replaces, one match at a time or all at once.
- **Ctrl+Shift+F** finds in every file of the project, with the same options as the find bar, and in the
  files a pattern names when one is given, such as `*.cpp *.h`. The matches fill a tab of their own as soon as
  the search ends, which goes on while you work; Enter on one opens its file there. Files open with unsaved
  changes are searched as you have them. Binary files and those over 16 MB are left out, and only the first
  2000 matches are listed.
- **Ctrl+G** goes to a line.

**Tools > Filter Through Command** runs a shell command with the selection, or the whole document when nothing
is selected, as its input, and puts what it prints in its place, as one step to undo: `sort -u`, `jq .`,
`clang-format`, `column -t`. A command that fails leaves the text alone and shows its error.

## Comparing files

**Tools > Compare Files...** compares the document you are editing with a file you choose, or two files chosen one
after the other when no document is open; **Tools > Compare with Saved** compares the document with its file as last
saved, to see what you are about to save. `oui_code --diff left right` opens a comparison from the command line,
as `git difftool` can ask for one.

The comparison is a tab of its own, the two texts side by side with the lines that are the same facing each other.
A line only on the left is red, removed; only on the right, green, added; on both but different, it is set apart,
with what changed within it in red on the left and green on the right. A document that is open is compared as you
have it, unsaved changes and all, and the comparison follows it as you edit.

| Key | Action |
|-----|--------|
| F7 / Shift+F7 | Next / previous difference |
| Up, Down, Page Up, Page Down, Home, End | Move through the rows |
| Left, Right | See more of long lines |
| Enter, or a double click | Go to that line, in its document; on the side clicked, the right one otherwise |

The bar under the texts says how many differences there are, and which one you are on.

## Scripts

OUI Code colours code and changes text with scripts in JavaScript. Its own define C++, JavaScript and
TypeScript, Python, GDScript, JSON, Markdown, CMake and shell scripts, the comments of a score of other
languages, and the commands in **Tools > Text Commands**: sort lines, remove duplicate lines, upper and lower
case, snake_case and camelCase, format and minify JSON. Each takes the selection, or the whole document when
nothing is selected. **Alt+X** lists them alone, to pick one by typing part of its name, as the palette does.

A document is coloured as the language of its file; **View > Syntax** picks another, or plain text, and the
status bar says which.

Yours go in the folder `oui_code_scripts` beside the settings: every `.js` file there runs after OUI Code's
own, in name order. **Tools > User Scripts > Open User Script** opens `user.js` there, made the first time with
examples, and **Reload User Scripts**, beside it, runs them all again after a change. A script that fails says so,
and why, when the scripts load. What they print with `console.log` is in **User Scripts Output**, beside them; a
script run from Tools has its own tab for that.

A command takes text and gives back what goes in its place, or nothing to leave it:

```js
oui.commands.register({
    id: "user.shout",
    title: "Shout",            // or {en: "Shout", es: "Gritar"}
    keys: "Ctrl+Alt+U",        // optional
    menuPath: "edit/Case",     // optional: Edit > Case > Shout
    run: text => text.toUpperCase(),
});
```

A command is in **Tools > Text Commands**, or where its `menuPath` puts it, written as a path of files. Its first
name is one of the menus, `file`, `edit`, `view`, `tools` or `help`, whatever language OUI Code is in; the rest are
submenus, made the first time one is named and shown as written. A submenu of OUI Code's own is found by its name
in English or as it shows: `tools/Text Commands` adds to that one. As with the title, it can be one per language:
`{en: "edit/Case", es: "edit/Mayúsculas"}`. Wherever it is, **Alt+X** and the palette list it too.

A language says which files are in it, what starts a comment, which brackets pair, and how to colour it:

```js
oui.languages.register({
    id: "ini",
    name: "INI",
    extensions: [".ini"],      // files: ["Makefile"] for whole names
    comment: ";",
    states: {
        start: [
            {match: /;.*/, kind: "comment"},
            {match: /^\s*\[.*\]/, kind: "heading"},
            {match: /"/, push: "string"},
        ],
        string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}]},
    },
});
```

Colouring goes through a line in states, starting in `start`; at each place the rule whose pattern matches
first wins, the earliest in the list on a tie. A rule can `push` another state, such as a string or a comment,
and `pop` back out of it; a state can last over many lines, as a block comment does. The kinds are keyword,
type, function, string, number, constant, comment, preprocessor, operator, tag, attribute, heading, emphasis,
link and invalid. `oui.words("if", "else")` makes the pattern for a list of whole words. `suggestions` lists what
the editor suggests while one writes in the language: words, or `{label, detail, text}` for a piece of text, with
`$0` where the cursor goes after. A definition with the id of another replaces it, so yours can change OUI Code's
own. Everything scripts can use is in `oui/oui.d.ts`, beside OUI Code's source.

### Running a script

Any script of yours, anywhere, can work on the document you are editing or on its own, such as on a whole
folder. It is a module that exports one of two functions, both told whether this is a dry run:

```js
// On the document: file is its whole path.
export function processFile(file, dryRun) {
    const text = oui.readFile(file);
    const tidy = text.replaceAll("\t", "    ");
    if (dryRun) {
        console.log(`would replace ${text.split("\t").length - 1} tabs`);
    } else {
        oui.writeFile(file, tidy);
    }
}

// On its own: folder is the project's, or the document's when there is no project.
export async function process(dryRun, folder) {
    for (const name of oui.listDir(folder).filter((name) => name.endsWith(".md"))) {
        console.log(dryRun ? "would check" : "checking", name);
        if (!dryRun) await oui.run(`markdownlint "${folder}/${name}"`);
    }
}
```

A dry run should change nothing and say what it would do; `dryRun` is not optional, and a script whose
function does not take it is refused. **Tools > Script Templates** writes a script of either kind where the
cursor is, to start from, and one of a third kind, which runs after Generate in an Interactive UI tab.

**Tools > Run Script...** asks which script, starting in the document's folder, checks that it is one of these,
and keeps it for the next time; loading it to check runs what the module does on loading, which in a script
like the ones above is only defining its functions. It then runs it as a dry run. **F5** runs the kept script
again as a dry run, and **Shift+F5** for real. A `processFile` in a dry run works on a copy of the document,
unsaved changes and all, so even one that writes anyway leaves the file alone, and what it made of the copy
opens in a tab beside; for real, the document is saved first and its tab then shows what the script changed. A
`process` has no copy to work on: its dry run is only as safe as the script makes it. After one for real, the
open documents and the folder panels show what it changed.

A tab shows what the script prints and the programs it runs say, with a button to stop those programs; a dry
run's is titled *Preview*. Each run takes the same tab over, and F5 or Shift+F5 pressed there runs the script
again on the same document. Closing the tab stops the run. A run ends when the function returns, or when the
promise of an `async` one settles.

Besides JavaScript, a script has `oui.readFile(path)`, `oui.writeFile(path, text)`, `oui.exists(path)`,
`oui.listDir(path)`, `await oui.run(command, {cwd, input, output})`, `await oui.message(title, text)`,
`await oui.confirm(question)`, `await oui.prompt(title, label, text)`, `await oui.choose(title, options)`,
`setTimeout` and `setInterval`, and can `import` modules beside it.

### Tools of your own

A script in the folder `tools`, inside `oui_code_scripts`, is a tool: a command of yours, in the menus and on its
keys, that works on what the editor has, with all a script run from Tools has. It exports `tool`, which says what it
is, and `run`, which gets the document's file and language, the project's folder, the selection, and the cursor's
line and column:

```js
// oui_code_scripts/tools/count_words.js
export const tool = {
    title: "Count Words",      // or {en: "Count Words", es: "Contar palabras"}
    keys: "Ctrl+Alt+W",        // optional
    menuPath: "edit",          // optional: in Tools > User Tools without it
};

export async function run({file, language, folder, selection, line, column}) {
    const words = selection.split(/\s+/).filter(Boolean).length;
    await oui.message("Count Words", `${words} words selected`);
}
```

`title`, `keys` and `menuPath` are as a text command's. The document is saved before the tool runs, so its file
says what the editor does; `file` is `null` for one never saved, `selection` is `""` when nothing is selected, and
`line` and `column` count from 1. Text `run` gives back replaces the selection, or goes in at the cursor, a change
that undo takes back; giving back nothing leaves the document as it is. What it prints, and what the programs it
runs say, opens the scripts' tab, with its button to stop them; a tool that says nothing opens nothing. After it,
the open documents and the folder panels show what it changed.

Tools load with the rest of your scripts, and **Reload User Scripts** picks up a new one, or a change to what its
`tool` says; each run loads the script again, so a change to `run` needs no reload. **Tools > Script Templates**
writes one to start from.

## Interactive scripts

**Tools > Interactive Script** opens a tab with a script on the left and, on the right, what it makes, run again a
moment after each change to the script or to what it works on. From a script (`.js`), it asks what to run on: another
open document, text pasted in the tab, which shows over the result to write or paste in, or nothing, for a script
that only prints. From any other document, a new script starts with an
example, to run on that document. Place the tab as you like: beside the rest, on a group of its own, alone.

- A script that exports `transform(text)` gets the document's text and gives back the text in its place: the result.
  **Apply** puts it in the document, a change that undo takes back; until then the document is left as it is.
- A `processFile(file, dryRun)` works on a copy of the document, as a dry run, and the result is the copy as it
  leaves it; a `process(dryRun, folder)` runs as a dry run too, and what it would do shows in what it prints.
- What it prints is under the result; **Log** shows it or hides it.
- When it fails, the error is along the bottom, with its line, which a click goes to; the result stays as it was
  until the script works again, marked as from before the error.
- It only looks, as it runs half typed: reading files and folders works, writing files, running programs and asking
  questions do not. Run it on its own, with Tools > Run Script, to do it for real.
- The code is a document as any other: **Ctrl+S** saves it, into the script it came from.

## Interactive user interfaces

**Tools > Interactive UI** opens a tab with a user interface description on the left and, on the right, the
interface it describes, built again a moment after each change, to try as it would be used: type in its fields, tick
its boxes, press its buttons. On a `.oui` file it works on that file; from anything else it starts a new description
from an example.

A description is a widget a line: its type, its text in quotes, then its properties, `name=value` or a name alone.
What goes in a widget is indented under it, and `#` starts a comment.

```
Window "Rename Files" width=50
  Label "&Pattern:" buddy=pattern
  LineEdit id=pattern placeholder="A regular expression"
  CheckButton "&Show what changes first" checked
  Row centered gap=tight
    Button "&Rename" default
    Button "Cancel"
```

- `Window`, `Column`, `Row`, `Grid`, `Split` and `Tabs` with `Tab` hold others; `Label`, `LineEdit`, `TextEdit`,
  `MarkdownView`, `CheckButton`, `Button`, `ComboBox`, `VSpacer`, `HSpacer` and `Slot` do not. A `Slot` is where a
  program or a script puts what a description cannot write, such as a list; here it shows its id.
- `id=name` is how the code finds a widget; `&` marks the letter Alt goes with; `buddy=id` is the field a label's
  letter goes to. A label with no buddy has nowhere to take it, and shows its text without the `&`; `&&` writes one.
- A description that names `context=` has its texts translated, as OUI Code's own windows are; one that does not
  shows them as written, as a script's does.
- As you write, the suggestions say what fits: the widgets at a line's start, after a widget and a space the
  properties it takes, after an = their values; Ctrl+Space asks for them anywhere.
- When it does not build, the line along the bottom says where and why, and a click goes there; what built last
  stays until it builds again.
- The description is a document as any other: **Ctrl+S** saves it. A script reads it with `oui.uiFromFile`, as the
  scripting guide tells.
- **Generate**, over the preview, writes code to start from in a new tab: the description's file loaded, each widget
  with an id wired to what it is usually wanted for (a button's click, a field's change), and the window opened. A
  description with no file yet is saved first, as the code loads it by its name. In the language beside it, which is
  remembered: **JavaScript**, a script to save beside the description and run with oui_run, or **C++**, a function to
  put in a program, with the file built in by `olagarro_embed_files` as the comment over it says. From the keyboard
  both are in the command palette.
- Beside Generate, a script of yours can run after it, one for each language, remembered: it gets what Generate
  wrote, to save it where your project wants it, build with it, or check it, as your project is set up. It is a
  module that exports `processGeneratedCode(code, {language, file, dryRun})`, `language` being `"javascript"` or
  `"cpp"` and `file` the description's whole path, with all a script run from Tools has (`oui.writeFile`,
  `await oui.run`...). What it prints, and what the programs it runs say, shows in the tab of a script run, and what
  it gives back, as text, at the end: whether it went well, what is missing.
- **New...**, in the same list, writes one: an Interactive Script tab with a script to start from, which writes the
  code beside the description unless a file is there already, run as you change it on what Generate writes now,
  pasted in the tab. There it runs as a dry run, `dryRun` true, and says what it would do; after Generate, `dryRun`
  is false and it does it. Saved, it is the script that runs after Generate in that language.

  ```js
  export async function processGeneratedCode(code, {language, file, dryRun}) {
      const target = file.replace(/\.oui$/, ".cpp");
      if (dryRun) return `Would write ${target} and build.`;
      oui.writeFile(target, code);
      const exit = await oui.run("cmake --build build");
      return exit === 0 ? "Built." : "The build failed: see above.";
  }
  ```

## Things to do

**View > Code TODOs** lists every TODO, FIXME, XXX and HACK in the project's files, as Find in Files lists its
matches; Enter on one opens its file there.

**View > Task List** keeps the project's own list of things to do, each a line that says what and a note with as
much as it needs, as a ticket would. The list is on the left and the note of the selected task on the right:

| Key | Action |
|-----|--------|
| Enter, Insert or the **+ Add Task** button | New task, after the selected one, with its priority |
| Space, or a click on its box | Done, or not done again: it moves to the other tab |
| F6 | Show the tasks done, or those pending |
| F2 | Rename |
| Alt+1 to Alt+4 | Urgent, high, normal or low priority |
| Delete | Delete, after asking |
| Alt+Up / Alt+Down | Move it up or down |
| Tab, or a double click | Write its note; Escape comes back to the list |
| Typing, or Ctrl+F | Find tasks by their title or note, in the field above the list |

Over the list, the **Pending** and **Done** tabs say how many there are of each. The pending tasks show in
your own order and those done the most recent first, until a click on the **Name** or **Priority** header sorts
them by it, up, then down, then back; moving tasks with Alt+Up and Alt+Down changes your order, so it works when the
pending list shows it whole and unsorted. A
right click on a task opens a menu with the same as the keys. Over the note, **Title** and **Priority** show and
change what the task is and how soon it wants doing; the list shows each task's box, name and priority in columns.

Every change is saved at once in `tasks.md` at the top of the project, as Markdown, to read or edit by hand or keep
in git: a `- [ ] title` line for each task, `- [x]` once done with the day it was done after it, its priority before the title as todo.txt writes it, `(A)`
urgent, `(B)` high and `(D)` low, with nothing for a normal one, and its note under it indented by two spaces. Without
a project the list is kept beside the settings. **Save As** keeps a list anywhere else, under a name that ends in
`.tasks.md`; opening a file named `tasks.md` or ending that way opens it as a task list again. Task lists come back
with the rest of the session.

## A shell in a tab

**Tools > New Terminal**, or **Ctrl** or **Alt** and the key under Escape (**Ctrl+`** or **Alt+`**, or **Ctrl+º**
or **Alt+º** on a Spanish keyboard), opens your shell in a tab, in the open folder; typing `exit` closes it. In a
terminal only Alt works: a terminal has no way to send Ctrl with that key.
While it has the focus it takes the keys a terminal program wants, Ctrl+C and Ctrl+W among them, but for
**Ctrl+Page Up**, **Ctrl+Page Down** and **Ctrl+Tab**, which still move between tabs, and **Ctrl+Shift** with a
letter, which stays OUI Code's: **Ctrl+Shift+V** pastes. **Shift+Page Up** and **Shift+Page Down**, and the
wheel, look back at what scrolled off. Dragging the mouse selects, a double click takes a word and a triple click a
line; what is selected is copied when the button is let go, and with **Ctrl+Shift+C**. A right click pastes.

## Saving and keeping your work

- **Ctrl+S** saves, **Ctrl+Shift+S** saves as, and **File > Save All** saves every changed tab.
- OUI Code notices when another program changes an open file, and reloads it or asks, as Preferences say.
- If the terminal closes or the connection drops with changes unsaved, they are kept and come back, still
  unsaved, the next time OUI Code starts.
- Every few seconds OUI Code also keeps a copy of the unsaved changes, so a crash or a power cut loses no more
  than that. Preferences turn it off or say how often, every 10 seconds unless told otherwise.
- With **Keep unsaved changes on exit** on, in Preferences, quitting asks nothing and everything comes back as
  it was, as in Sublime Text.
- The settings, `oui_code.ini`, and what is kept of unsaved work are beside OUI Code itself when its folder can be
  written, so it carries them along on a USB stick; otherwise, as when it is installed for the whole system, in
  your configuration folder: `~/.config/oui_code` on Linux, `%APPDATA%\oui_code` on Windows.
- **View > Theme** picks the look in the terminal too: Turbo C++'s blue, Night, Light, or Monochrome for a terminal
  of few colours. It is kept for the next time.

## In a window

OUI Code in its GUI kind also opens in a desktop window: from the desktop's menu, or typing `oui_code --window` in
a terminal.
Everything works as in the terminal, and the keys a terminal keeps for itself reach it. **View > Theme** picks its
look, and **Ctrl++**, **Ctrl+-** and **Ctrl+0** make the text bigger, smaller or as it was; it keeps both for the
next time. Tabs can be dragged by their title, to another place among their group's or to another group. Typed in a terminal without `--window`, it runs in the terminal, like the terminal kind.

## Terminals

Some key combinations never reach a program running in a terminal:

- **Ctrl+M**, **Ctrl+I**, **Ctrl+J** and **Ctrl+[** arrive as Return, Tab, a new line and Escape. Terminals
  that cannot tell Ctrl+Shift and a letter from Ctrl and the letter send **Ctrl+Shift+M** as Return too; the
  Edit menu has the command as well.
- A terminal may keep **Alt** and a letter, or **Ctrl+Shift** and a letter, for its own menus. Konsole in
  Spanish keeps Alt+A, for instance; use F10 to reach the menus.
- The keypad's keys may lose Ctrl.

On the Linux console OUI Code adapts to its 8 background colours and its font. To see in any terminal how it
looks there, start it as `OUI_LINUX_CONSOLE=1 oui_code`.
