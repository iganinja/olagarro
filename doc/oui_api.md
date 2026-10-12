# OUI script API

*tools/extract_api.js makes this file from the comments marked "api" beside the code that defines each name:
change those, and run it again, rather than this file.*

Everything OUI's scripts can use besides JavaScript itself, name by name. [Scripting](scripting.md) shows
how to put it to work; `oui/oui.d.ts` has the same as TypeScript declarations, for editors that read them.

Not every script has every name: each entry ends saying which have it.

## Contents

- [Globals](#globals): [clearInterval](#clearinterval), [clearTimeout](#cleartimeout), [setInterval](#setinterval), [setTimeout](#settimeout)
- [console](#console): [log](#consolelog)
- [oui](#oui): [args](#ouiargs), [settings](#ouisettings), [choose](#ouichoose), [chooseFolder](#ouichoosefolder), [column](#ouicolumn), [confirm](#ouiconfirm), [copy](#ouicopy), [exists](#ouiexists), [exit](#ouiexit), [fill](#ouifill), [listDir](#ouilistdir), [menuBar](#ouimenubar), [message](#ouimessage), [openExternally](#ouiopenexternally), [openFile](#ouiopenfile), [paste](#ouipaste), [prompt](#ouiprompt), [quit](#ouiquit), [readFile](#ouireadfile), [row](#ouirow), [run](#ouirun), [saveFile](#ouisavefile), [shortcut](#ouishortcut), [split](#ouisplit), [status](#ouistatus), [theme](#ouitheme), [ui](#ouiui), [uiFromFile](#ouiuifromfile), [window](#ouiwindow), [window](#ouiwindow), [words](#ouiwords), [writeFile](#ouiwritefile), [Box](#ouibox), [Button](#ouibutton), [Calculator](#ouicalculator), [Canvas](#ouicanvas), [CheckButton](#ouicheckbutton), [ComboBox](#ouicombobox), [Comparison](#ouicomparison), [Dialog](#ouidialog), [Dock](#ouidock), [FolderView](#ouifolderview), [Grid](#ouigrid), [HSpacer](#ouihspacer), [Label](#ouilabel), [LineEdit](#ouilineedit), [List](#ouilist), [MarkdownView](#ouimarkdownview), [Panel](#ouipanel), [ProgressBar](#ouiprogressbar), [ScrollView](#ouiscrollview), [Split](#ouisplit-1), [Table](#ouitable), [Tabs](#ouitabs), [Terminal](#ouiterminal), [TextEdit](#ouitextedit), [Tree](#ouitree), [Ui](#ouiui-1), [VSpacer](#ouivspacer), [Widget](#ouiwidget)
- [oui.commands](#ouicommands): [register](#ouicommandsregister)
- [oui.languages](#ouilanguages): [register](#ouilanguagesregister)

## Globals

### clearInterval

```ts
clearInterval(id: number): void
```

Stops what setInterval started.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### clearTimeout

```ts
clearTimeout(id: number): void
```

Stops what setTimeout started, if it is still to come.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### setInterval

```ts
setInterval(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number
```

Calls fn with args every ms milliseconds, until clearInterval with what it gives.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### setTimeout

```ts
setTimeout(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number
```

Calls fn with args once, ms milliseconds from now; clearTimeout with what it gives stops it.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

## console

### console.log

```ts
console.log(...values: any[]): void
```

Writes the values on a line, apart by spaces: to the terminal in oui_run, to the output tab in OUI Code.

*In: Every script.*

## oui

### oui.args

```ts
oui.args: string[]
```

The arguments after the script's name.

*In: oui_run.*

### oui.settings

```ts
oui.settings: {get(key: string, fallback?: any): any, set(key: string, value: any): void, remove(key: string): void}
```

What the script keeps from one run to the next, by names of its choosing: any value JSON holds. In <script>.ini, in the user's configuration folder for oui_run, ~/.config/oui_run on Linux; scripts with the same name share it.

*In: oui_run.*

### oui.choose

```ts
oui.choose(title: string, options: (string | {label: string, detail?: string})[]): Promise<string | null>
```

Asks to pick one of options, and gives its label; null when cancelled. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.chooseFolder

```ts
oui.chooseFolder(options?: {title?: string, folder?: string}): Promise<string | null>
```

As oui.openFile, for a folder.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.column

```ts
oui.column(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box
```

The children one under the other; one given through oui.fill takes the room left.

*In: oui_run.*

### oui.confirm

```ts
oui.confirm(question: string): Promise<boolean>
```

Asks yes or no. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.copy

```ts
oui.copy(text: string): void
```

Puts text on the clipboard, the system's where there is one. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.exists

```ts
oui.exists(path: string): boolean
```

Whether there is a file or a folder at path.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.exit

```ts
oui.exit(code?: number): void
```

Ends the program there and then, with code, 0 unless given: nothing after it runs, not even a catch or a finally.

*In: oui_run.*

### oui.fill

```ts
oui.fill(widget: oui.Widget): {fill: oui.Widget}
```

A widget that takes the room a column or a row leaves.

*In: oui_run.*

### oui.listDir

```ts
oui.listDir(path: string): string[]
```

The names in a folder, in order; a folder's ends in "/".

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.menuBar

```ts
oui.menuBar(menus: {title: string, items: ({title: string, keys?: string, run: () => void} | "-")[]}[]): void
```

The window's menus along its top, each a title and its entries: an entry a title, the keys that run it too, as "Ctrl+O", and what it runs; "-" a line between entries. & marks the letter Alt goes with. Before or after oui.window; asked again, the new menus take the old ones' place.

*In: oui_run.*

### oui.message

```ts
oui.message(title: string, text?: string): Promise<void>
```

Tells something, in a window when there is one and on the terminal when not. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.openExternally

```ts
oui.openExternally(target: string): boolean
```

Hands a web address or a file to what the system opens it with, as a browser, and does not wait for it; false when the system could not be asked. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.openFile

```ts
oui.openFile(options?: {title?: string, folder?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>
```

Asks for a file to open, in the file dialog: from folder, the current one unless given; filters as {name: "Scripts", patterns: "*.js;*.ts"}. Gives its path, with / between folders on every system; null when cancelled. Without a window, its path is asked on the terminal. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.paste

```ts
oui.paste(): string
```

What the clipboard holds, as text.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.prompt

```ts
oui.prompt(title: string, label?: string, text?: string, options?: {password?: boolean}): Promise<string | null>
```

Asks for a line of text, starting with text; null when cancelled. For a password, what is typed shows as dots, or not at all when asked on the terminal. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.quit

```ts
oui.quit(): void
```

Ends the program once what it is doing now is done.

*In: oui_run.*

### oui.readFile

```ts
oui.readFile(path: string): string | null
```

The file's text, or null when it cannot be read.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.row

```ts
oui.row(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box
```

The children side by side; one given through oui.fill takes the room left.

*In: oui_run.*

### oui.run

```ts
oui.run(command: string, options?: {cwd?: string, input?: string, output?: (text: string) => void}): Promise<number> & {stop(): void}
```

Runs a shell command and gives its exit code. Its output goes to the terminal, or to output as it comes; input is what it reads; cwd the folder it runs in. stop() on the promise stops it. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.saveFile

```ts
oui.saveFile(options?: {title?: string, folder?: string, name?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>
```

As oui.openFile, for a file to save as, name offered: it asks before one that is there is replaced.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.shortcut

```ts
oui.shortcut(keys: string, fn: () => void): void
```

Calls fn when keys are pressed in the window, written as "Ctrl+S" or "Alt+Shift+F2".

*In: oui_run.*

### oui.split

```ts
oui.split(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number}): oui.Split
```

new oui.Split, as a function beside oui.column and oui.row.

*In: oui_run.*

### oui.status

```ts
oui.status(text: string, right?: string): void
```

What the line along the bottom of the window says, and what it says at its right end; the line shows from the first time.

*In: oui_run.*

### oui.theme

```ts
oui.theme(name: string): void
```

The look: "turbo", "night", "light", "monochrome", "amber", "phosphor" or "commander" in a terminal, and "classic" in a window too.

*In: oui_run.*

### oui.ui

```ts
oui.ui(description: string): oui.Ui
```

Builds the widgets a description says, as a .oui file of OUI's writes them: a widget a line, what goes in it indented under it, and its properties after it.

*In: oui_run.*

### oui.uiFromFile

```ts
oui.uiFromFile(path: string): oui.Ui
```

oui.ui of the file at path, found from the script's folder when it is not whole, wherever the script is run from.

*In: oui_run.*

### oui.window

```ts
oui.window(title: string, content?: oui.Widget): void
```

Opens the window, titled title, with content in it: any widget, as a column of others.

*In: oui_run.*

### oui.window

```ts
oui.window(ui: oui.Ui): void
```

Opens the window a description's Window says: its title, and what it holds.

*In: oui_run.*

### oui.words

```ts
oui.words(...lists: string[][]): RegExp
```

A regular expression for any of the words, whole words only, as keyword rules want: oui.words(["if", "else"]).

*In: OUI Code's language and command scripts, and oui_run.*

### oui.writeFile

```ts
oui.writeFile(path: string, text: string): boolean
```

Writes text as the file, whether it was there or not; false when it cannot. Not in an interactive script.

*In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.*

### oui.Box

```ts
class oui.Box extends oui.Widget
```

What oui.column and oui.row make.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `add(widget: oui.Widget, options?: {fill?: boolean}): this`: Puts widget after the others; with fill it takes the room they leave.

### oui.Button

```ts
class oui.Button extends oui.Widget
new oui.Button(text: string, onClick?: () => void, options?: {default?: boolean})
```

A button, which calls its onClick when pressed.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `text: string`: What it says, & marking the letter Alt presses it with.
- `onClick: () => void`: What pressing it does.

### oui.Calculator

```ts
class oui.Calculator extends oui.Widget
new oui.Calculator(options?: {onExit?: () => void})
```

OUI's calculator: what is written on its line worked out, as oui_calc does.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

### oui.Canvas

```ts
class oui.Canvas extends oui.Widget
new oui.Canvas(options?: {width?: number, height?: number, onMouse?: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void, onResize?: (width: number, height: number) => void})
```

What a script draws on, in cells, as text and filled areas: what is drawn stays until clear(). Of a size of its own, or of all the room it has.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `width: number`: How wide it is now, in cells; height, how tall.
- `height: number`
- `clear(background?: string): this`: Forgets what was drawn, its background a colour as widgets take them, the theme's unless given.
- `text(x: number, y: number, text: string, style?: {color?: string, background?: string, bold?: boolean, underline?: boolean}): this`: Writes text from the cell at x, y, counted from 0 at its top left corner.
- `fill(x: number, y: number, width: number, height: number, background: string): this`: Fills cells with a colour.
- `onMouse: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void`: Called with what the pointer does over it: the cell under it, from the canvas's corner, and what it did. A press with clicks 2 is a double click; wheel is how far the wheel turned.
- `onResize: (width: number, height: number) => void`: Called when its size changes, the first time too: where to draw it again.

### oui.CheckButton

```ts
class oui.CheckButton extends oui.Widget
new oui.CheckButton(text: string, options?: {checked?: boolean, onToggle?: (checked: boolean) => void})
```

A box to tick, with its text beside it.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `checked: boolean`: Whether it is ticked.
- `onToggle: (checked: boolean) => void`: Called when it is ticked or unticked.

### oui.ComboBox

```ts
class oui.ComboBox extends oui.Widget
new oui.ComboBox(items?: string[], options?: {onSelect?: (index: number) => void})
```

One of a few options, the others in a list that opens.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `items: string[]`: Its options.
- `selected: number`: Which option is chosen, from 0.
- `onSelect: (index: number) => void`: Called when another option is chosen.

### oui.Comparison

```ts
class oui.Comparison extends oui.Widget
new oui.Comparison(options?: {left?: string, right?: string, leftTitle?: string, rightTitle?: string})
```

Two texts side by side, what differs between them marked, as OUI Code compares files.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `compare(left: string, right: string, leftTitle?: string, rightTitle?: string): this`: Shows the two texts, under their titles.
- `differences: number`: How many places differ.
- `next(): this`: Goes to the next difference; previous() to the one before.

### oui.Dialog

```ts
class oui.Dialog
new oui.Dialog(title: string, content: oui.Widget, options?: {width?: number, height?: number, resizable?: boolean})
```

A window of the script's own over the main one, which waits on it: what it holds, under its title. Escape or its close button cancels it. It opens again as often as it is shown, with its widgets as they were left.

*In: oui_run.*

- `show(): Promise<any>`: Opens it and waits for it to close: what close was given, or null when it was cancelled.
- `close(value?: any): void`: Closes it, show giving back value, true unless given.

### oui.Dock

```ts
class oui.Dock extends oui.Widget
new oui.Dock()
```

Widgets against its edges, as a window's bars and side panels, and one in the room they leave.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `add(side: "top" | "bottom" | "left" | "right" | "fill", widget: oui.Widget, options?: {size?: number, resizable?: number}): this`: Each takes its strip of the room the ones before left; size in cells, its own unless given; a side strip with resizable is made wider by the mouse, keeping that many cells at least.

### oui.FolderView

```ts
class oui.FolderView extends oui.Widget
new oui.FolderView(folder?: string, options?: {onActivate?: (path: string) => void})
```

A folder's files and folders, as OUI Code's explorer: Enter goes into a folder, Backspace up.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `folder: string`: The folder shown, whole.
- `selected: string | null`: The path of what is chosen in it.
- `onActivate: (path: string) => void`: Called with a file chosen with Enter or a double click.

### oui.Grid

```ts
class oui.Grid extends oui.Widget
new oui.Grid(options?: {columns?: (number | "content" | "fill" | "flex" | string)[], rows?: (number | "content" | "fill" | "flex" | string)[], gap?: "none" | "tight" | "normal" | "loose"})
```

Widgets in columns and rows, a row at a time, as a description's Grid.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `add(...widgets: oui.Widget[]): this`: Puts the widgets in, after what is there.

### oui.HSpacer

```ts
class oui.HSpacer extends oui.Widget
new oui.HSpacer(width: number)
```

Room across, of a width in cells.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

### oui.Label

```ts
class oui.Label extends oui.Widget
new oui.Label(text?: string, options?: {bold?: boolean, wrap?: boolean})
```

A line of text, or a paragraph when it wraps.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `text: string`: What it says.

### oui.LineEdit

```ts
class oui.LineEdit extends oui.Widget
new oui.LineEdit(options?: {text?: string, placeholder?: string, password?: boolean, onChange?: (text: string) => void, onSubmit?: (text: string) => void})
```

A field of one line.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `text: string`: What is written in it.
- `onChange: (text: string) => void`: Called at every change.
- `onSubmit: (text: string) => void`: Called when Enter is pressed in it.
- `placeholder: string`: What it shows, quieter, while it is empty.
- `password: boolean`: Whether it is for a password: a dot shows for each character, copying and cutting take nothing, and Control with an arrow goes to an end rather than to a word's. Its text is what was typed.
- `selection: [number, number]`: What is selected, from start to end, as places in its text; the same twice where the cursor is.
- `select(start: number, end?: number): this`: Selects from start to end, the cursor at end; with one place, only puts the cursor there.
- `selectAll(): this`
- `selectedText: string`
- `insert(text: string): this`: Writes text in place of the selection, as typing would.

### oui.List

```ts
class oui.List extends oui.Widget
new oui.List(items?: string[], options?: {checks?: boolean, onSelect?: (index: number) => void, onActivate?: (index: number) => void, onToggle?: (index: number, checked: boolean) => void})
```

A list to pick from, with the keyboard or the mouse.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `items: string[]`: What it lists.
- `selected: number`: Which item is selected, from 0.
- `onSelect: (index: number) => void`: Called when another item is selected.
- `onActivate: (index: number) => void`: Called when an item is chosen, with Enter or a double click.
- `checked: boolean[]`: With checks, whether each item's box is ticked; setting items unticks them all.
- `onToggle: (index: number, checked: boolean) => void`: With checks, called when a box is ticked or unticked.

### oui.MarkdownView

```ts
class oui.MarkdownView extends oui.Widget
new oui.MarkdownView(markdown?: string, options?: {onLink?: (target: string) => void})
```

Markdown to read: headings, lists, tables, code.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `markdown: string`: What it shows.
- `onLink: (target: string) => void`: Called when a link that leads out of the text is followed, with where it leads, as written: a web address to hand to oui.openExternally, or a file to show. One to a section of the text, "#its-heading", the view goes to itself.
- `goTo(section: string): boolean`: Shows the heading a link to its section names at the top, "installing" for "## Installing"; false when there is none.

### oui.Panel

```ts
class oui.Panel extends oui.Widget
new oui.Panel(content: oui.Widget, options?: {padding?: number | [number, number, number, number]})
```

What it holds, with room round it, on the panels' colour.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

### oui.ProgressBar

```ts
class oui.ProgressBar extends oui.Widget
new oui.ProgressBar(options?: {value?: number, text?: string})
```

How far something has gone, as a bar that fills from the left, with a text after it, as "4 of 8".

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `value: number`: How much is done, from 0, nothing, to 1, all of it; kept within them.
- `text: string`: What it says after the bar; nothing when empty.

### oui.ScrollView

```ts
class oui.ScrollView extends oui.Widget
new oui.ScrollView(content: oui.Widget)
```

What it holds, scrolled when it is taller than the room it has.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

### oui.Split

```ts
class oui.Split extends oui.Widget
new oui.Split(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number})
```

Two widgets with a line between them that the mouse drags: side by side, or one over the other when vertical.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `ratio: number`: The first one's share, from 0 to 1: half unless given.

### oui.Table

```ts
class oui.Table extends oui.Widget
new oui.Table(headings: string[], options?: {widths?: number[], rows?: (string | number)[][], onSelect?: (index: number) => void, onActivate?: (index: number) => void, onSort?: (column: number, order: "ascending" | "descending" | "none") => void})
```

Rows of cells under headings. A click on a heading asks for the rows in its order, ascending, then descending, then as they were: onSort hears it, and the script puts its rows in that order and sets them again.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `rows: (string | number)[][]`: What it shows, a row an array of cells. Set again, the row chosen and where it was scrolled to stay.
- `selected: number`: Which row is chosen, from 0.
- `onSelect: (index: number) => void`: Called when another row is chosen.
- `onActivate: (index: number) => void`: Called when a row is chosen with Enter or a double click.
- `onSort: (column: number, order: "ascending" | "descending" | "none") => void`: Called when a heading is clicked, with the order it asks for.
- `sortBy(column: number, order: "ascending" | "descending" | "none"): this`: Marks the heading of the column the rows are in the order of, as a click on it would.
- `widths: number[]`: The columns' widths, in cells.
- `stretch: number | null`: The column that takes the room the others leave: the last unless set; null for none.
- `resizable: boolean`: Whether dragging the edge between two headings makes a column wider or narrower.
- `headings: boolean`: Whether its headings show.
- `show(row: number): this`: Scrolls so the row is in sight.
- `onResize: (column: number, width: number) => void`: Called when a column was made wider or narrower with the mouse.

### oui.Tabs

```ts
class oui.Tabs extends oui.Widget
new oui.Tabs(options?: {fixed?: boolean, onSelect?: (index: number) => void})
```

Pages one at a time, each with its title on a tab over them.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `add(title: string, widget: oui.Widget): this`: Adds a page after the others.
- `count: number`: How many pages it has.
- `selected: number`: The page on show, from 0.
- `onSelect: (index: number) => void`: Called when another page shows.
- `pages: oui.Widget[]`: Its pages, in order.
- `title(index: number): string`: The title of a page; setTitle(index, title) changes it.
- `setTitle(index: number, title: string): this`
- `remove(index: number): oui.Widget`: Takes a page out, and gives it, to go elsewhere or nowhere.
- `move(from: number, to: number): this`: Moves a page among the others; the page on show stays on show.
- `marked: boolean[]`: Which pages ask for attention, their close mark in the warning colour, as documents not saved.
- `onClose: (index: number) => void`: Called when a page's close mark is pressed: the page goes unless this is set, and then only if it says so with remove(index).

### oui.Terminal

```ts
class oui.Terminal extends oui.Widget
new oui.Terminal(options?: {command?: string[], folder?: string, onFinished?: (code: number) => void, onTitle?: (title: string) => void})
```

A program running in a terminal of its own, the user's shell unless told another: what is typed goes to it.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `paste(text: string): this`: Sends text as pasted.
- `running: boolean`: Whether its program is still running.

### oui.TextEdit

```ts
class oui.TextEdit extends oui.Widget
new oui.TextEdit(options?: {text?: string, readOnly?: boolean, wrap?: boolean, lineNumbers?: boolean, onChange?: (text: string) => void})
```

A text of many lines, to write or only to show.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `text: string`: All of its text.
- `readOnly: boolean`: Whether it can only be read.
- `append(text: string): void`: Adds text at its end, as output that comes.
- `onChange: (text: string) => void`: Called at every change.
- `wrap: boolean`: Whether lines too long for it go on below rather than off its side.
- `lineNumbers: boolean`
- `cursor: number`: Where the cursor is, as a place in its text; set, the cursor goes there.
- `line: number`: The cursor's line, from 1; column is its place in the line, from 1.
- `column: number`
- `selection: [number, number]`: What is selected, from start to end, as places in its text; the same twice where the cursor is.
- `select(start: number, end?: number): this`: Selects from start to end, the cursor at end; with one place, only puts the cursor there.
- `selectAll(): this`
- `selectedText: string`
- `insert(text: string): this`: Writes text in place of the selection, as typing would: one step to undo, and nothing when it is read only.
- `modified: boolean`: Whether it changed since it was set false, as a document saved.
- `find(text: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean, backward?: boolean}): boolean`: Selects the next place with text from the cursor, or the one before it backward, going round past the end; false when there is none. Case matters unless matchCase is false; regex takes text as a regular expression.
- `replace(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): boolean`: Replaces the selection when it is text, and selects the next one; false when there is none.
- `replaceAll(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number`: Replaces every one, as one step to undo, and gives how many.
- `showMatches(text: string | null, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number`: Marks every place with text, as a find bar does, until called with null; gives how many there are.
- `goToLine(line: number): boolean`: Puts the cursor at the start of the line, from 1; false when there is no such line.
- `undo(): boolean`: Undoes the last change; false when there is none. redo() does it again.
- `redo(): boolean`
- `duplicateLines(): boolean`: The lines the selection touches, or the cursor's: copied below them. moveLines("up" or "down") moves them, deleteLines() deletes them, toggleComment(prefix) comments them out with prefix, or back.
- `moveLines(direction: "up" | "down"): boolean`
- `deleteLines(): boolean`
- `toggleComment(prefix: string): boolean`
- `tabSpaces: number`: How many spaces Tab writes, with useSpaces; a tab otherwise.
- `useSpaces: boolean`
- `autoIndent: boolean`: Whether Enter starts the new line as indented as the one before.
- `occurrences: boolean`: Whether the other places with the selected word are marked.
- `highlight: {[state: string]: object[] | {kind: string, rules: object[]}} | null`: Colours the text by rules, written as an OUI Code language's states: {start: [{match: oui.words(["if"]), kind: "keyword"}, {match: /"/, push: "string"}], string: {kind: "string", rules: [{match: /"/, pop: true}]}}. null leaves it in one colour.
- `onCursor: () => void`: Called when the cursor or the selection moves.

### oui.Tree

```ts
class oui.Tree extends oui.Widget
new oui.Tree(items?: {text: string, children?: object[], expanded?: boolean}[], options?: {onSelect?: (path: number[]) => void, onActivate?: (path: number[]) => void})
```

Nodes in a tree, each with those under it, which open and close: a node is where it is, its places from the top, [1, 0] the first under the second.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

- `items: {text: string, children?: object[], expanded?: boolean}[]`: What it shows: each node its text, the nodes under it as nodes too, and whether it starts open.
- `selected: number[] | null`: Where the node chosen is; set, the nodes above it open.
- `onSelect: (path: number[]) => void`: Called when another node is chosen.
- `onActivate: (path: number[]) => void`: Called when a node is chosen with Enter or a double click.

### oui.Ui

```ts
class oui.Ui
```

The widgets of a description: each with an id by its id, as the object of its kind, ui.go a oui.Button. An id cannot be root or put.

*In: oui_run.*

- `root: oui.Widget`: The widget the description starts with, to put in a window or anywhere else.
- `put(id: string, widget: oui.Widget): oui.Widget`: Puts widget where the description has the Slot with the id, which then names it: a widget a description cannot write, as a oui.List.

### oui.VSpacer

```ts
class oui.VSpacer extends oui.Widget
new oui.VSpacer(height: number)
```

Room down, of a height in cells.

*In: oui_run.*

Has what [oui.Widget](#ouiwidget) has, and:

### oui.Widget

```ts
class oui.Widget
```

What every widget is: a label, a button, a field, a column...

*In: oui_run.*

- `id: number`: What the host knows it by.
- `focus(): this`: Gives it the keyboard; one that cannot have it, as a column or a calculator, gives it to the first of its widgets that can.
- `visible: boolean`: Whether it shows; one that does not takes no room either.
- `enabled: boolean`: Whether it answers: a disabled widget shows so, and takes no click, no key and no focus. What a disabled one holds is disabled with it.
- `tooltip: string`: What shows by the pointer resting on it.
- `background: string`: The colour behind it, one of the theme's by its name, "table_background", or a fixed one, "#20a0ff". Labels, fields, buttons, boxes to tick, Markdown views and trees have one.
- `color: string`: The colour of its text, as background takes one. Labels and Markdown views have one.
- `remove(): this`: Takes it out of what it is in, to put it in another or nowhere; a page of tabs, or a side of a split, stays.
- `contextMenu: ({title: string, keys?: string, run: () => void} | "-")[]`: The entries a right click on it opens, as a menu's, where the pointer is.
- `onKey: (key: string, text: string) => boolean | void`: Hears a key before the widget does, while it or what it holds has the focus: the key as shortcuts write it, "Ctrl+S" or "Enter", and the text it types, if any; true keeps it from going further.

## oui.commands

### oui.commands.register

```ts
oui.commands.register(command: {id: string, title: string | {[language: string]: string}, keys?: string, menuPath?: string | {[language: string]: string}, run: (text: string) => string | undefined}): void
```

Adds a text command, in the palette and Alt+X, on keys when given: run gets the selection, or the whole document when nothing is selected, and gives back what goes in its place, or nothing to leave it. It shows in Tools > Text Commands, or where menuPath puts it, as a path of files: "edit" in the Edit menu, "edit/Case" in a submenu Case of it, made when there is none. The first name is the menu's, file, edit, view, tools or help, whatever language OUI Code is in; the rest are submenus as they show, OUI Code's own found by their name in English or as translated. Like the title, it can be one per language: {en: "edit/Case", es: "edit/Mayúsculas"}.

*In: OUI Code's language and command scripts.*

## oui.languages

### oui.languages.register

```ts
oui.languages.register(language: {id: string, name?: string, extensions?: string[], files?: string[], comment?: string, brackets?: string[], states: {[state: string]: object[]}, suggestions?: (string | {label: string, detail?: string, text?: string})[]}): void
```

Adds a language, or replaces one with the same id: how files of its extensions and names are coloured, from the rules of each state, how a line is commented, which brackets pair, and what it suggests as one writes: words, as its keywords, or pieces of text with a name, where "$0" marks where the cursor goes.

*In: OUI Code's language and command scripts.*
