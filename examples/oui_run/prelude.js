// The classes and functions oui_run's scripts see as `oui`, built on the plain calls the host defines as `__oui`,
// which it then hides, and on what every system script has, `__ouiSystem`. It runs before the script, as a script
// of its own: what it defines stays out of the script's way, in a block. What each name is, for
// tools/extract_api.js to put in oui/oui.d.ts:
//
// api(oui_run): oui.args: string[]
// The arguments after the script's name.
// api(oui_run): oui.window(title: string, content?: oui.Widget): void
// Opens the window, titled title, with content in it: any widget, as a column of others.
// api(oui_run): oui.window(ui: oui.Ui): void
// Opens the window a description's Window says: its title, and what it holds.
// api(oui_run): oui.ui(description: string): oui.Ui
// Builds the widgets a description says, as a .oui file of OUI's writes them: a widget a line, what goes in it
// indented under it, and its properties after it.
//
//     Window "Rename"
//       Label "&Pattern:" buddy=pattern
//       LineEdit id=pattern placeholder="*.txt"
//       CheckButton "&Dry run" id=dry checked
//       Row centered gap=tight
//         Button "Rename" id=go default
//
// Its texts show as they are written. One that is not a description throws, saying the line and the column wrong.
// api(oui_run): oui.uiFromFile(path: string): oui.Ui
// oui.ui of the file at path, found from the script's folder when it is not whole, wherever the script is run from.
// api(oui_run): class oui.Ui
// The widgets of a description: each with an id by its id, as the object of its kind, ui.go a oui.Button. An id
// cannot be root or put.
// api(oui_run): oui.Ui#root: oui.Widget
// The widget the description starts with, to put in a window or anywhere else.
// api(oui_run): oui.Ui#put(id: string, widget: oui.Widget): oui.Widget
// Puts widget where the description has the Slot with the id, which then names it: a widget a description cannot
// write, as a oui.List.
// api(oui_run): oui.shortcut(keys: string, fn: () => void): void
// Calls fn when keys are pressed in the window, written as "Ctrl+S" or "Alt+Shift+F2".
// api(oui_run): oui.quit(): void
// Ends the program once what it is doing now is done.
// api(oui_run): oui.exit(code?: number): void
// Ends the program there and then, with code, 0 unless given: nothing after it runs, not even a catch or a finally.
// api(oui_run): oui.column(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box
// The children one under the other; one given through oui.fill takes the room left.
// api(oui_run): oui.row(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box
// The children side by side; one given through oui.fill takes the room left.
// api(oui_run): oui.fill(widget: oui.Widget): {fill: oui.Widget}
// A widget that takes the room a column or a row leaves.
// api(oui_run): oui.split(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number}): oui.Split
// new oui.Split, as a function beside oui.column and oui.row.
// api(oui_run): class oui.Widget
// What every widget is: a label, a button, a field, a column...
// api(oui_run): oui.Widget#id: number
// What the host knows it by.
// api(oui_run): oui.Widget#focus(): this
// Gives it the keyboard; one that cannot have it, as a column or a calculator, gives it to the first of its widgets
// that can.
// api(oui_run): oui.Widget#visible: boolean
// Whether it shows; one that does not takes no room either.
// api(oui_run): oui.Widget#enabled: boolean
// Whether it answers: a disabled widget shows so, and takes no click, no key and no focus. What a disabled one holds
// is disabled with it.
// api(oui_run): oui.Widget#tooltip: string
// What shows by the pointer resting on it.
// api(oui_run): oui.Widget#background: string
// The colour behind it, one of the theme's by its name, "table_background", or a fixed one, "#20a0ff". Labels,
// fields, buttons, boxes to tick, Markdown views and trees have one.
// api(oui_run): oui.Widget#color: string
// The colour of its text, as background takes one. Labels and Markdown views have one.
// api(oui_run): oui.Widget#remove(): this
// Takes it out of what it is in, to put it in another or nowhere; a page of tabs, or a side of a split, stays.
// api(oui_run): oui.Widget#contextMenu: ({title: string, keys?: string, run: () => void} | "-")[]
// The entries a right click on it opens, as a menu's, where the pointer is.
// api(oui_run): oui.Widget#onKey: (key: string, text: string) => boolean | void
// Hears a key before the widget does, while it or what it holds has the focus: the key as shortcuts write it,
// "Ctrl+S" or "Enter", and the text it types, if any; true keeps it from going further.
// api(oui_run): oui.setLanguage(language: string): void
// The language OUI's own texts are in, as a dialog's buttons and the file dialog: "en" or "es", the system's until
// told. A script written in one language says it first, before its window, so all it shows is in that one.
// api(oui_run): oui.language(): string
// The language OUI's own texts are in, as "en" or "es".
// api(oui_run): oui.setTheme(name: string): void
// The look: "turbo", "night", "light", "monochrome", "amber", "phosphor" or "commander" in a terminal, and "classic"
// in a window too.
// api(oui_run): oui.theme(): string
// The look in use, by its name: the one chosen, or "classic" in a desktop window and "turbo" in a terminal.
// api(oui_run): class oui.Box extends oui.Widget
// What oui.column and oui.row make.
// api(oui_run): oui.Box#add(widget: oui.Widget, options?: {fill?: boolean}): this
// Puts widget after the others; with fill it takes the room they leave.
// api(oui_run): class oui.Grid extends oui.Widget
// Widgets in columns and rows, a row at a time, as a description's Grid.
// api(oui_run): new oui.Grid(options?: {columns?: (number | "content" | "fill" | "flex" | string)[], rows?: (number | "content" | "fill" | "flex" | string)[], gap?: "none" | "tight" | "normal" | "loose"})
// A length is a number of cells, as wide as what is in it, the rest, a share of the rest, or "40%".
// api(oui_run): oui.Grid#add(...widgets: oui.Widget[]): this
// Puts the widgets in, after what is there.
// api(oui_run): class oui.ScrollView extends oui.Widget
// What it holds, scrolled when it is taller than the room it has.
// api(oui_run): new oui.ScrollView(content: oui.Widget)
// api(oui_run): class oui.Panel extends oui.Widget
// What it holds, with room round it, on the panels' colour.
// api(oui_run): new oui.Panel(content: oui.Widget, options?: {padding?: number | [number, number, number, number]})
// padding in cells: one for every side, or top, right, bottom and left.
// api(oui_run): class oui.Dock extends oui.Widget
// Widgets against its edges, as a window's bars and side panels, and one in the room they leave.
// api(oui_run): new oui.Dock()
// api(oui_run): oui.Dock#add(side: "top" | "bottom" | "left" | "right" | "fill", widget: oui.Widget, options?: {size?: number, resizable?: number}): this
// Each takes its strip of the room the ones before left; size in cells, its own unless given; a side strip with
// resizable is made wider by the mouse, keeping that many cells at least.
// api(oui_run): class oui.HSpacer extends oui.Widget
// Room across, of a width in cells.
// api(oui_run): new oui.HSpacer(width: number)
// api(oui_run): class oui.VSpacer extends oui.Widget
// Room down, of a height in cells.
// api(oui_run): new oui.VSpacer(height: number)
// api(oui_run): class oui.Comparison extends oui.Widget
// Two texts side by side, what differs between them marked, as OUI Code compares files.
// api(oui_run): new oui.Comparison(options?: {left?: string, right?: string, leftTitle?: string, rightTitle?: string})
// api(oui_run): oui.Comparison#compare(left: string, right: string, leftTitle?: string, rightTitle?: string): this
// Shows the two texts, under their titles.
// api(oui_run): oui.Comparison#differences: number
// How many places differ.
// api(oui_run): oui.Comparison#next(): this
// Goes to the next difference; previous() to the one before.
// api(oui_run): class oui.FolderView extends oui.Widget
// A folder's files and folders, as OUI Code's explorer: Enter goes into a folder, Backspace up.
// api(oui_run): new oui.FolderView(folder?: string, options?: {onActivate?: (path: string) => void})
// api(oui_run): oui.FolderView#folder: string
// The folder shown, whole.
// api(oui_run): oui.FolderView#selected: string | null
// The path of what is chosen in it.
// api(oui_run): oui.FolderView#onActivate: (path: string) => void
// Called with a file chosen with Enter or a double click.
// api(oui_run): class oui.Terminal extends oui.Widget
// A program running in a terminal of its own, the user's shell unless told another: what is typed goes to it.
// api(oui_run): new oui.Terminal(options?: {command?: string[], folder?: string, onFinished?: (code: number) => void, onTitle?: (title: string) => void})
// api(oui_run): oui.Terminal#paste(text: string): this
// Sends text as pasted.
// api(oui_run): oui.Terminal#running: boolean
// Whether its program is still running.
// api(oui_run): class oui.Calculator extends oui.Widget
// OUI's calculator: what is written on its line worked out, as oui_calc does.
// api(oui_run): new oui.Calculator(options?: {onExit?: () => void})
// api(oui_run): class oui.Dialog
// A window of the script's own over the main one, which waits on it: what it holds, under its title. Escape or its
// close button cancels it. It opens again as often as it is shown, with its widgets as they were left.
// api(oui_run): new oui.Dialog(title: string, content: oui.Widget, options?: {width?: number, height?: number, resizable?: boolean})
// width and height in cells, the least it opens at; resizable lets the mouse make it bigger.
// api(oui_run): new oui.Dialog(ui: oui.Ui)
// The dialog a description's Window says: its title, its size, and what it holds.
// api(oui_run): oui.Dialog#show(): Promise<any>
// Opens it and waits for it to close: what close was given, or null when it was cancelled.
// api(oui_run): oui.Dialog#close(value?: any): void
// Closes it, show giving back value, true unless given.
// api(oui_run): oui.setMenuBar(menus: {title: string, items: ({title: string, keys?: string, run: () => void} | "-")[]}[]): void
// The window's menus along its top, each a title and its entries: an entry a title, the keys that run it too, as
// "Ctrl+O", and what it runs; "-" a line between entries. & marks the letter Alt goes with. Before or after
// oui.window; asked again, the new menus take the old ones' place.
// api(oui_run): oui.setStatus(text: string, right?: string): void
// What the line along the bottom of the window says, and what it says at its right end; the line shows from the
// first time.
// api(oui_run): class oui.Label extends oui.Widget
// A line of text, or a paragraph when it wraps.
// api(oui_run): new oui.Label(text?: string, options?: {bold?: boolean, wrap?: boolean})
// api(oui_run): oui.Label#text: string
// What it says.
// api(oui_run): class oui.Button extends oui.Widget
// A button, which calls its onClick when pressed.
// api(oui_run): oui.Button#text: string
// What it says, & marking the letter Alt presses it with.
// api(oui_run): new oui.Button(text: string, onClick?: () => void, options?: {default?: boolean})
// A default button is the one Enter presses in a dialog.
// api(oui_run): oui.Button#onClick: () => void
// What pressing it does.
// api(oui_run): class oui.LineEdit extends oui.Widget
// A field of one line.
// api(oui_run): new oui.LineEdit(options?: {text?: string, placeholder?: string, password?: boolean, onChange?: (text: string) => void, onSubmit?: (text: string) => void})
// api(oui_run): oui.LineEdit#text: string
// What is written in it.
// api(oui_run): oui.LineEdit#onChange: (text: string) => void
// Called at every change.
// api(oui_run): oui.LineEdit#onSubmit: (text: string) => void
// Called when Enter is pressed in it.
// api(oui_run): oui.LineEdit#placeholder: string
// What it shows, quieter, while it is empty.
// api(oui_run): oui.LineEdit#password: boolean
// Whether it is for a password: a dot shows for each character, copying and cutting take nothing, and Control with an
// arrow goes to an end rather than to a word's. Its text is what was typed.
// api(oui_run): oui.LineEdit#selection: [number, number]
// What is selected, from start to end, as places in its text; the same twice where the cursor is.
// api(oui_run): oui.LineEdit#select(start: number, end?: number): this
// Selects from start to end, the cursor at end; with one place, only puts the cursor there.
// api(oui_run): oui.LineEdit#selectAll(): this
// api(oui_run): oui.LineEdit#selectedText: string
// api(oui_run): oui.LineEdit#insert(text: string): this
// Writes text in place of the selection, as typing would.
// api(oui_run): class oui.ProgressBar extends oui.Widget
// How far something has gone, as a bar that fills from the left, with a text after it, as "4 of 8".
// api(oui_run): new oui.ProgressBar(options?: {value?: number, text?: string})
// api(oui_run): oui.ProgressBar#value: number
// How much is done, from 0, nothing, to 1, all of it; kept within them.
// api(oui_run): oui.ProgressBar#text: string
// What it says after the bar; nothing when empty.
// api(oui_run): class oui.TextEdit extends oui.Widget
// A text of many lines, to write or only to show.
// api(oui_run): new oui.TextEdit(options?: {text?: string, readOnly?: boolean, wrap?: boolean, lineNumbers?: boolean, onChange?: (text: string) => void})
// api(oui_run): oui.TextEdit#text: string
// All of its text.
// api(oui_run): oui.TextEdit#readOnly: boolean
// Whether it can only be read.
// api(oui_run): oui.TextEdit#append(text: string): void
// Adds text at its end, as output that comes.
// api(oui_run): oui.TextEdit#onChange: (text: string) => void
// Called at every change.
// api(oui_run): oui.TextEdit#wrap: boolean
// Whether lines too long for it go on below rather than off its side.
// api(oui_run): oui.TextEdit#lineNumbers: boolean
// api(oui_run): oui.TextEdit#cursor: number
// Where the cursor is, as a place in its text; set, the cursor goes there.
// api(oui_run): oui.TextEdit#line: number
// The cursor's line, from 1; column is its place in the line, from 1.
// api(oui_run): oui.TextEdit#column: number
// api(oui_run): oui.TextEdit#selection: [number, number]
// What is selected, from start to end, as places in its text; the same twice where the cursor is.
// api(oui_run): oui.TextEdit#select(start: number, end?: number): this
// Selects from start to end, the cursor at end; with one place, only puts the cursor there.
// api(oui_run): oui.TextEdit#selectAll(): this
// api(oui_run): oui.TextEdit#selectedText: string
// api(oui_run): oui.TextEdit#insert(text: string): this
// Writes text in place of the selection, as typing would: one step to undo, and nothing when it is read only.
// api(oui_run): oui.TextEdit#modified: boolean
// Whether it changed since it was set false, as a document saved.
// api(oui_run): oui.TextEdit#find(text: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean, backward?: boolean}): boolean
// Selects the next place with text from the cursor, or the one before it backward, going round past the end;
// false when there is none. Case matters unless matchCase is false; regex takes text as a regular expression.
// api(oui_run): oui.TextEdit#replace(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): boolean
// Replaces the selection when it is text, and selects the next one; false when there is none.
// api(oui_run): oui.TextEdit#replaceAll(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number
// Replaces every one, as one step to undo, and gives how many.
// api(oui_run): oui.TextEdit#showMatches(text: string | null, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number
// Marks every place with text, as a find bar does, until called with null; gives how many there are.
// api(oui_run): oui.TextEdit#goToLine(line: number): boolean
// Puts the cursor at the start of the line, from 1; false when there is no such line.
// api(oui_run): oui.TextEdit#undo(): boolean
// Undoes the last change; false when there is none. redo() does it again.
// api(oui_run): oui.TextEdit#redo(): boolean
// api(oui_run): oui.TextEdit#duplicateLines(): boolean
// The lines the selection touches, or the cursor's: copied below them. moveLines("up" or "down") moves them,
// deleteLines() deletes them, toggleComment(prefix) comments them out with prefix, or back.
// api(oui_run): oui.TextEdit#moveLines(direction: "up" | "down"): boolean
// api(oui_run): oui.TextEdit#deleteLines(): boolean
// api(oui_run): oui.TextEdit#toggleComment(prefix: string): boolean
// api(oui_run): oui.TextEdit#tabSpaces: number
// How many spaces Tab writes, with useSpaces; a tab otherwise.
// api(oui_run): oui.TextEdit#useSpaces: boolean
// api(oui_run): oui.TextEdit#autoIndent: boolean
// Whether Enter starts the new line as indented as the one before.
// api(oui_run): oui.TextEdit#occurrences: boolean
// Whether the other places with the selected word are marked.
// api(oui_run): oui.TextEdit#highlight: {[state: string]: object[] | {kind: string, rules: object[]}} | null
// Colours the text by rules, written as an OUI Code language's states: {start: [{match: oui.words(["if"]), kind:
// "keyword"}, {match: /"/, push: "string"}], string: {kind: "string", rules: [{match: /"/, pop: true}]}}. null
// leaves it in one colour.
// api(oui_run): oui.TextEdit#onCursor: () => void
// Called when the cursor or the selection moves.
// api(oui_run): class oui.CheckButton extends oui.Widget
// A box to tick, with its text beside it.
// api(oui_run): new oui.CheckButton(text: string, options?: {checked?: boolean, onToggle?: (checked: boolean) => void})
// api(oui_run): oui.CheckButton#checked: boolean
// Whether it is ticked.
// api(oui_run): oui.CheckButton#onToggle: (checked: boolean) => void
// Called when it is ticked or unticked.
// api(oui_run): class oui.ComboBox extends oui.Widget
// One of a few options, the others in a list that opens.
// api(oui_run): new oui.ComboBox(items?: string[], options?: {onSelect?: (index: number) => void})
// api(oui_run): oui.ComboBox#items: string[]
// Its options.
// api(oui_run): oui.ComboBox#selected: number
// Which option is chosen, from 0.
// api(oui_run): oui.ComboBox#onSelect: (index: number) => void
// Called when another option is chosen.
// api(oui_run): class oui.Tabs extends oui.Widget
// Pages one at a time, each with its title on a tab over them.
// api(oui_run): new oui.Tabs(options?: {fixed?: boolean, onSelect?: (index: number) => void})
// api(oui_run): oui.Tabs#add(title: string, widget: oui.Widget): this
// Adds a page after the others.
// api(oui_run): oui.Tabs#count: number
// How many pages it has.
// api(oui_run): oui.Tabs#selected: number
// The page on show, from 0.
// api(oui_run): oui.Tabs#onSelect: (index: number) => void
// Called when another page shows.
// api(oui_run): oui.Tabs#pages: oui.Widget[]
// Its pages, in order.
// api(oui_run): oui.Tabs#title(index: number): string
// The title of a page; setTitle(index, title) changes it.
// api(oui_run): oui.Tabs#setTitle(index: number, title: string): this
// api(oui_run): oui.Tabs#remove(index: number): oui.Widget
// Takes a page out, and gives it, to go elsewhere or nowhere.
// api(oui_run): oui.Tabs#move(from: number, to: number): this
// Moves a page among the others; the page on show stays on show.
// api(oui_run): oui.Tabs#marked: boolean[]
// Which pages ask for attention, their close mark in the warning colour, as documents not saved.
// api(oui_run): oui.Tabs#onClose: (index: number) => void
// Called when a page's close mark is pressed: the page goes unless this is set, and then only if it says so with
// remove(index).
// api(oui_run): class oui.Split extends oui.Widget
// Two widgets with a line between them that the mouse drags: side by side, or one over the other when vertical.
// api(oui_run): new oui.Split(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number})
// api(oui_run): oui.Split#ratio: number
// The first one's share, from 0 to 1: half unless given.
// api(oui_run): class oui.MarkdownView extends oui.Widget
// Markdown to read: headings, lists, tables, code.
// api(oui_run): new oui.MarkdownView(markdown?: string, options?: {onLink?: (target: string) => void})
// api(oui_run): oui.MarkdownView#markdown: string
// What it shows.
// api(oui_run): oui.MarkdownView#onLink: (target: string) => void
// Called when a link that leads out of the text is followed, with where it leads, as written: a web address to hand
// to oui.openExternally, or a file to show. One to a section of the text, "#its-heading", the view goes to itself.
// api(oui_run): oui.MarkdownView#goTo(section: string): boolean
// Shows the heading a link to its section names at the top, "installing" for "## Installing"; false when there is
// none.
// api(oui_run): class oui.List extends oui.Widget
// A list to pick from, with the keyboard or the mouse.
// api(oui_run): new oui.List(items?: string[], options?: {checks?: boolean, onSelect?: (index: number) => void, onActivate?: (index: number) => void, onToggle?: (index: number, checked: boolean) => void})
// With checks, each item has a box before it, which a click on it, or Space on the item chosen, ticks.
// api(oui_run): oui.List#items: string[]
// What it lists.
// api(oui_run): oui.List#selected: number
// Which item is selected, from 0.
// api(oui_run): oui.List#onSelect: (index: number) => void
// Called when another item is selected.
// api(oui_run): oui.List#onActivate: (index: number) => void
// Called when an item is chosen, with Enter or a double click.
// api(oui_run): oui.List#checked: boolean[]
// With checks, whether each item's box is ticked; setting items unticks them all.
// api(oui_run): oui.List#onToggle: (index: number, checked: boolean) => void
// With checks, called when a box is ticked or unticked.
// api(oui_run): class oui.Tree extends oui.Widget
// Nodes in a tree, each with those under it, which open and close: a node is where it is, its places from the top,
// [1, 0] the first under the second.
// api(oui_run): new oui.Tree(items?: {text: string, children?: object[], expanded?: boolean}[], options?: {onSelect?: (path: number[]) => void, onActivate?: (path: number[]) => void})
// api(oui_run): oui.Tree#items: {text: string, children?: object[], expanded?: boolean}[]
// What it shows: each node its text, the nodes under it as nodes too, and whether it starts open.
// api(oui_run): oui.Tree#selected: number[] | null
// Where the node chosen is; set, the nodes above it open.
// api(oui_run): oui.Tree#onSelect: (path: number[]) => void
// Called when another node is chosen.
// api(oui_run): oui.Tree#onActivate: (path: number[]) => void
// Called when a node is chosen with Enter or a double click.
// api(oui_run): class oui.Table extends oui.Widget
// Rows of cells under headings. A click on a heading asks for the rows in its order, ascending, then descending,
// then as they were: onSort hears it, and the script puts its rows in that order and sets them again.
// api(oui_run): new oui.Table(headings: string[], options?: {widths?: number[], rows?: (string | number)[][], onSelect?: (index: number) => void, onActivate?: (index: number) => void, onSort?: (column: number, order: "ascending" | "descending" | "none") => void})
// The headings, and the widths of the columns in cells: every one but the last, which takes the rest.
// api(oui_run): oui.Table#rows: (string | number)[][]
// What it shows, a row an array of cells. Set again, the row chosen and where it was scrolled to stay.
// api(oui_run): oui.Table#selected: number
// Which row is chosen, from 0.
// api(oui_run): oui.Table#onSelect: (index: number) => void
// Called when another row is chosen.
// api(oui_run): oui.Table#onActivate: (index: number) => void
// Called when a row is chosen with Enter or a double click.
// api(oui_run): oui.Table#onSort: (column: number, order: "ascending" | "descending" | "none") => void
// Called when a heading is clicked, with the order it asks for.
// api(oui_run): oui.Table#sortBy(column: number, order: "ascending" | "descending" | "none"): this
// Marks the heading of the column the rows are in the order of, as a click on it would.
// api(oui_run): oui.Table#widths: number[]
// The columns' widths, in cells.
// api(oui_run): oui.Table#stretch: number | null
// The column that takes the room the others leave: the last unless set; null for none.
// api(oui_run): oui.Table#resizable: boolean
// Whether dragging the edge between two headings makes a column wider or narrower.
// api(oui_run): oui.Table#headings: boolean
// Whether its headings show.
// api(oui_run): oui.Table#show(row: number): this
// Scrolls so the row is in sight.
// api(oui_run): oui.Table#onResize: (column: number, width: number) => void
// Called when a column was made wider or narrower with the mouse.
// api(oui_run): class oui.Canvas extends oui.Widget
// What a script draws on, in cells, as text and filled areas: what is drawn stays until clear(). Of a size of its
// own, or of all the room it has.
// api(oui_run): new oui.Canvas(options?: {width?: number, height?: number, onMouse?: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void, onResize?: (width: number, height: number) => void})
// api(oui_run): oui.Canvas#width: number
// How wide it is now, in cells; height, how tall.
// api(oui_run): oui.Canvas#height: number
// api(oui_run): oui.Canvas#clear(background?: string): this
// Forgets what was drawn, its background a colour as widgets take them, the theme's unless given.
// api(oui_run): oui.Canvas#text(x: number, y: number, text: string, style?: {color?: string, background?: string, bold?: boolean, underline?: boolean}): this
// Writes text from the cell at x, y, counted from 0 at its top left corner.
// api(oui_run): oui.Canvas#fill(x: number, y: number, width: number, height: number, background: string): this
// Fills cells with a colour.
// api(oui_run): oui.Canvas#onMouse: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void
// Called with what the pointer does over it: the cell under it, from the canvas's corner, and what it did. A press
// with clicks 2 is a double click; wheel is how far the wheel turned.
// api(oui_run): oui.Canvas#onResize: (width: number, height: number) => void
// Called when its size changes, the first time too: where to draw it again.
// api(oui_run): oui.settings: {value(key: string, fallback?: any): any, setValue(key: string, value: any): void, remove(key: string): void}
// What the script keeps from one run to the next, by names of its choosing: any value JSON holds. In
// <script>.ini, in the user's configuration folder for oui_run, ~/.config/oui_run on Linux; scripts with the same
// name share it.
{
const n = globalThis.__oui;
delete globalThis.__oui;

class Widget {
    constructor(id) { Object.defineProperty(this, "id", {value: id}); }
    focus() { n.focus(this.id); return this; }
    get visible() { return n.getVisible(this.id); }
    set visible(value) { n.setVisible(this.id, !!value); }
    get enabled() { return n.getEnabled(this.id); }
    set enabled(value) { n.setEnabled(this.id, !!value); }
    set tooltip(text) { n.setTooltip(this.id, String(text)); }
    set background(name) { n.setColor(this.id, "background", String(name)); }
    set color(name) { n.setColor(this.id, "color", String(name)); }
    remove() { n.remove(this.id); return this; }
    set contextMenu(items) { n.contextMenu(this.id, menuEntries(items)); }
    set onKey(fn) { n.onKey(this.id, fn); }
}

// A menu's entries as the host takes them: {title, keys, run}, or "-" for a line.
const menuEntries = (items) => (items ?? []).map((item) => item === "-" ? "-"
    : {title: String(item.title), keys: item.keys === undefined ? undefined : String(item.keys), run: item.run});

class Box extends Widget {
    add(widget, options = {}) {
        n.boxAdd(this.id, widget.id, !!options.fill);
        return this;
    }
}

class Grid extends Widget {
    constructor(options = {}) {
        super(n.grid(options.columns ?? [], options.rows ?? [], String(options.gap ?? "tight")));
    }
    add(...widgets) {
        for (const widget of widgets) n.containerAdd(this.id, widget.id);
        return this;
    }
}

class ScrollView extends Widget {
    constructor(content) {
        super(n.scrollView());
        n.containerAdd(this.id, content.id);
    }
}

class Panel extends Widget {
    constructor(content, options = {}) {
        super(n.panel(options.padding ?? 0));
        n.containerAdd(this.id, content.id);
    }
}

class Dock extends Widget {
    constructor() { super(n.dock()); }
    add(side, widget, options = {}) {
        n.dockAdd(this.id, widget.id, String(side), Number(options.size ?? 0), Number(options.resizable ?? 0));
        return this;
    }
}

class HSpacer extends Widget {
    constructor(width) { super(n.hspacer(Number(width))); }
}

class VSpacer extends Widget {
    constructor(height) { super(n.vspacer(Number(height))); }
}

class Comparison extends Widget {
    constructor(options = {}) {
        super(n.comparison());
        if (options.left !== undefined || options.right !== undefined) {
            this.compare(options.left ?? "", options.right ?? "", options.leftTitle, options.rightTitle);
        }
    }
    compare(left, right, leftTitle = "", rightTitle = "") {
        n.setComparison(this.id, String(left), String(right), String(leftTitle), String(rightTitle));
        return this;
    }
    get differences() { return n.comparisonCount(this.id); }
    next() { n.comparisonStep(this.id, true); return this; }
    previous() { n.comparisonStep(this.id, false); return this; }
}

class FolderView extends Widget {
    constructor(folder = ".", options = {}) {
        super(n.folderView(String(folder)));
        if (options.onActivate) this.onActivate = options.onActivate;
    }
    get folder() { return n.getFolder(this.id); }
    set folder(path) { n.setFolder(this.id, String(path)); }
    get selected() { return n.getFolderSelected(this.id); }
    set onActivate(fn) { n.on(this.id, "activate", fn); }
}

class Terminal extends Widget {
    constructor(options = {}) {
        super(n.terminal((options.command ?? []).map(String), String(options.folder ?? "")));
        if (options.onFinished) n.on(this.id, "finished", options.onFinished);
        if (options.onTitle) n.on(this.id, "title", options.onTitle);
    }
    paste(text) { n.terminalPaste(this.id, String(text)); return this; }
    get running() { return n.terminalRunning(this.id); }
}

class Dialog {
    #id;
    #value = null;
    constructor(title, content, options = {}) {
        this.#id = title instanceof Ui ? Ui.dialogOf(title) : n.dialog(String(title), content.id, options);
    }
    async show() {
        this.#value = null;
        const accepted = await n.dialogShow(this.#id);
        return accepted ? this.#value : null;
    }
    close(value = true) {
        this.#value = value;
        n.dialogClose(this.#id, true);
    }
}

class Calculator extends Widget {
    constructor(options = {}) {
        super(n.calculator());
        if (options.onExit) n.on(this.id, "exit", options.onExit);
    }
}

class Label extends Widget {
    constructor(text = "", options = {}) {
        super(n.label(String(text)));
        if (options.bold) n.setBold(this.id, true);
        if (options.wrap) n.setWrap(this.id, true);
    }
    get text() { return n.getText(this.id); }
    set text(value) { n.setText(this.id, String(value)); }
}

class ProgressBar extends Widget {
    constructor(options = {}) {
        super(n.progressBar());
        if (options.value !== undefined) this.value = options.value;
        if (options.text !== undefined) this.text = options.text;
    }
    get value() { return n.getValue(this.id); }
    set value(value) { n.setValue(this.id, Number(value)); }
    get text() { return n.getText(this.id); }
    set text(value) { n.setText(this.id, String(value)); }
}

class Button extends Widget {
    constructor(text, onClick, options = {}) {
        super(n.button(String(text)));
        if (onClick) this.onClick = onClick;
        if (options.default) n.setDefault(this.id, true);
    }
    get text() { return n.getText(this.id); }
    set text(value) { n.setText(this.id, String(value)); }
    set onClick(fn) { n.on(this.id, "click", fn); }
}

class LineEdit extends Widget {
    #placeholder = "";
    #password = false;
    constructor(options = {}) {
        super(n.lineEdit());
        if (options.text !== undefined) this.text = options.text;
        if (options.placeholder) this.placeholder = options.placeholder;
        if (options.password) this.password = true;
        if (options.onChange) this.onChange = options.onChange;
        if (options.onSubmit) this.onSubmit = options.onSubmit;
    }
    get text() { return n.getText(this.id); }
    set text(value) { n.setText(this.id, String(value)); }
    get placeholder() { return this.#placeholder; }
    set placeholder(value) {
        this.#placeholder = String(value);
        n.setPlaceholder(this.id, this.#placeholder);
    }
    get password() { return this.#password; }
    set password(value) {
        this.#password = Boolean(value);
        n.setPassword(this.id, this.#password);
    }
    get selection() { return n.getSelection(this.id); }
    select(start, end = start) { n.select(this.id, Number(start), Number(end)); return this; }
    selectAll() { n.selectAll(this.id); return this; }
    get selectedText() { return n.getSelectedText(this.id); }
    insert(text) { n.insert(this.id, String(text)); return this; }
    set onChange(fn) { n.on(this.id, "change", fn); }
    set onSubmit(fn) { n.on(this.id, "submit", fn); }
}

class TextEdit extends Widget {
    // What it was told, for what the editor cannot be asked.
    #told = {readOnly: false, wrap: false, lineNumbers: false, tabSpaces: 4, useSpaces: true, autoIndent: false,
             occurrences: false, highlight: null};
    constructor(options = {}) {
        super(n.textEdit());
        this.lineNumbers = !!options.lineNumbers;
        if (options.text !== undefined) this.text = options.text;
        if (options.readOnly) this.readOnly = true;
        if (options.wrap) this.wrap = true;
        if (options.onChange) this.onChange = options.onChange;
    }
    get text() { return n.getText(this.id); }
    set text(value) { n.setText(this.id, String(value)); }
    get readOnly() { return this.#told.readOnly; }
    set readOnly(value) { this.#told.readOnly = !!value; n.setReadOnly(this.id, !!value); }
    get wrap() { return this.#told.wrap; }
    set wrap(value) { this.#told.wrap = !!value; n.setWrap(this.id, !!value); }
    get lineNumbers() { return this.#told.lineNumbers; }
    set lineNumbers(value) { this.#told.lineNumbers = !!value; n.setLineNumbers(this.id, !!value); }
    append(text) { n.append(this.id, String(text)); }
    set onChange(fn) { n.on(this.id, "change", fn); }
    set onCursor(fn) { n.on(this.id, "cursor", fn); }
    get cursor() { return n.getCursor(this.id); }
    set cursor(index) { n.select(this.id, Number(index), Number(index)); }
    get line() { return this.text.slice(0, this.cursor).split("\n").length; }
    get column() {
        const before = this.text.slice(0, this.cursor);
        return before.length - before.lastIndexOf("\n");
    }
    get selection() { return n.getSelection(this.id); }
    select(start, end = start) { n.select(this.id, Number(start), Number(end)); return this; }
    selectAll() { n.selectAll(this.id); return this; }
    get selectedText() { return n.getSelectedText(this.id); }
    insert(text) { n.insert(this.id, String(text)); return this; }
    get modified() { return n.getModified(this.id); }
    set modified(value) { n.setModified(this.id, !!value); }
    find(text, options = {}) { return n.find(this.id, String(text), options); }
    replace(text, replacement, options = {}) { return n.replace(this.id, String(text), String(replacement), options); }
    replaceAll(text, replacement, options = {}) {
        return n.replaceAll(this.id, String(text), String(replacement), options);
    }
    showMatches(text, options = {}) { return n.showMatches(this.id, text === null ? null : String(text), options); }
    goToLine(line) { return n.goToLine(this.id, Number(line)); }
    undo() { return n.undo(this.id); }
    redo() { return n.redo(this.id); }
    duplicateLines() { return n.editLines(this.id, "duplicate"); }
    moveLines(direction) { return n.editLines(this.id, direction === "down" ? "down" : "up"); }
    deleteLines() { return n.editLines(this.id, "delete"); }
    toggleComment(prefix) { return n.editLines(this.id, "comment", String(prefix)); }
    #option(name, value) {
        this.#told[name] = value;
        n.setEditOption(this.id, name, value);
    }
    get tabSpaces() { return this.#told.tabSpaces; }
    set tabSpaces(value) { this.#option("tabSpaces", Number(value)); }
    get useSpaces() { return this.#told.useSpaces; }
    set useSpaces(value) { this.#option("useSpaces", !!value); }
    get autoIndent() { return this.#told.autoIndent; }
    set autoIndent(value) { this.#option("autoIndent", !!value); }
    get occurrences() { return this.#told.occurrences; }
    set occurrences(value) { this.#option("occurrences", !!value); }
    get highlight() { return this.#told.highlight; }
    set highlight(states) {
        n.setHighlight(this.id, states ?? null);
        this.#told.highlight = states ?? null;
    }
}

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

class ComboBox extends Widget {
    constructor(items = [], options = {}) {
        super(n.comboBox());
        this.items = items;
        if (options.onSelect) this.onSelect = options.onSelect;
    }
    set items(values) { n.setComboItems(this.id, values.map(String)); }
    get selected() { return n.getComboSelected(this.id); }
    set selected(index) { n.setComboSelected(this.id, index); }
    set onSelect(fn) { n.on(this.id, "select", fn); }
}

class List extends Widget {
    constructor(items = [], options = {}) {
        super(options.checks ? n.checkList() : n.list());
        this.items = items;
        if (options.onSelect) this.onSelect = options.onSelect;
        if (options.onActivate) this.onActivate = options.onActivate;
        if (options.onToggle) this.onToggle = options.onToggle;
    }
    get checked() { return n.getChecks(this.id); }
    set checked(values) { n.setChecks(this.id, values.map((value) => !!value)); }
    set onToggle(fn) { n.on(this.id, "toggle", fn); }
    set items(values) { n.setItems(this.id, values.map(String)); }
    get selected() { return n.getSelected(this.id); }
    set selected(index) { n.setSelected(this.id, index); }
    set onSelect(fn) { n.on(this.id, "select", fn); }
    set onActivate(fn) { n.on(this.id, "activate", fn); }
}

class Tree extends Widget {
    constructor(items = [], options = {}) {
        super(n.tree());
        this.items = items;
        if (options.onSelect) this.onSelect = options.onSelect;
        if (options.onActivate) this.onActivate = options.onActivate;
    }
    set items(values) {
        const node = (item) => ({text: String(item.text), expanded: !!item.expanded,
                                 children: (item.children ?? []).map(node)});
        n.setTreeItems(this.id, values.map(node));
    }
    get selected() { return n.getTreePath(this.id); }
    set selected(path) { n.setTreePath(this.id, path.map(Number)); }
    set onSelect(fn) { n.on(this.id, "select", fn); }
    set onActivate(fn) { n.on(this.id, "activate", fn); }
}

class Table extends Widget {
    #told = {resizable: false, headings: true, stretch: null};
    constructor(headings, options = {}) {
        super(n.table(headings.map(String), (options.widths ?? []).map(Number)));
        this.#told.stretch = headings.length > 0 ? headings.length - 1 : null;
        this.rows = options.rows ?? [];
        if (options.onSelect) this.onSelect = options.onSelect;
        if (options.onActivate) this.onActivate = options.onActivate;
        if (options.onSort) this.onSort = options.onSort;
    }
    set rows(values) { n.setRows(this.id, values.map((row) => row.map(String))); }
    get selected() { return n.getSelected(this.id); }
    set selected(index) { n.setSelected(this.id, index); }
    set onSelect(fn) { n.on(this.id, "select", fn); }
    set onActivate(fn) { n.on(this.id, "activate", fn); }
    set onSort(fn) { n.on(this.id, "sort", fn); }
    sortBy(column, order) {
        n.setTableSort(this.id, column, order);
        return this;
    }
    get widths() { return n.getColumnWidths(this.id); }
    set widths(values) { n.setTableOption(this.id, "widths", values.map(Number)); }
    get stretch() { return this.#told.stretch; }
    set stretch(column) {
        n.setTableOption(this.id, "stretch", column === null ? null : Number(column));
        this.#told.stretch = column === null ? null : Number(column);
    }
    get resizable() { return this.#told.resizable; }
    set resizable(value) { this.#told.resizable = !!value; n.setTableOption(this.id, "resizable", !!value); }
    get headings() { return this.#told.headings; }
    set headings(value) { this.#told.headings = !!value; n.setTableOption(this.id, "headers", !!value); }
    show(row) { n.showRow(this.id, Number(row)); return this; }
    set onResize(fn) { n.on(this.id, "resize", fn); }
}

class Canvas extends Widget {
    constructor(options = {}) {
        super(n.canvas(Number(options.width ?? 0), Number(options.height ?? 0)));
        if (options.onMouse) this.onMouse = options.onMouse;
        if (options.onResize) this.onResize = options.onResize;
    }
    get width() { return n.canvasSize(this.id)[0]; }
    get height() { return n.canvasSize(this.id)[1]; }
    clear(background) { n.canvasClear(this.id, background === undefined ? null : String(background)); return this; }
    text(x, y, text, style = {}) {
        const written = String(text);
        n.canvasDraw(this.id, Number(x), Number(y), written.length, 1, written, style);
        return this;
    }
    fill(x, y, width, height, background) {
        n.canvasDraw(this.id, Number(x), Number(y), Number(width), Number(height), "", {background: String(background)});
        return this;
    }
    set onMouse(fn) {
        n.on(this.id, "mouse", (x, y, kind, right, clicks, wheel, shift, control, alt) =>
            fn({x, y, kind, right, clicks, wheel, shift, control, alt}));
    }
    set onResize(fn) { n.on(this.id, "resize", fn); }
}

class Tabs extends Widget {
    #pages = [];
    #marked = [];
    constructor(options = {}) {
        super(n.tabs(!!options.fixed));
        if (options.onSelect) this.onSelect = options.onSelect;
        // A page whose close mark is pressed goes, unless onClose says otherwise.
        n.on(this.id, "close", (index) => this.remove(index));
        if (options.onClose) this.onClose = options.onClose;
    }
    add(title, widget) {
        n.tabsAdd(this.id, String(title), widget.id);
        this.#pages.push(widget);
        return this;
    }
    get pages() { return [...this.#pages]; }
    title(index) { return n.getTabTitle(this.id, Number(index)); }
    setTitle(index, title) { n.setTabTitle(this.id, Number(index), String(title)); return this; }
    remove(index) {
        n.tabsRemove(this.id, Number(index));
        this.#marked.splice(index, 1);
        n.setTabsMarked(this.id, this.#marked);
        return this.#pages.splice(index, 1)[0];
    }
    move(from, to) {
        n.tabsMove(this.id, Number(from), Number(to));
        this.#pages.splice(to, 0, ...this.#pages.splice(from, 1));
        this.#marked.splice(to, 0, ...this.#marked.splice(from, 1));
        n.setTabsMarked(this.id, this.#marked);
        return this;
    }
    get marked() { return this.#pages.map((_, index) => !!this.#marked[index]); }
    set marked(flags) {
        this.#marked = flags.map(Boolean);
        n.setTabsMarked(this.id, this.#marked);
    }
    set onClose(fn) { n.on(this.id, "close", fn); }
    get count() { return n.tabsCount(this.id); }
    get selected() { return n.getTabsSelected(this.id); }
    set selected(index) { n.setTabsSelected(this.id, index); }
    set onSelect(fn) { n.on(this.id, "select", fn); }
}

class Split extends Widget {
    constructor(first, second, options = {}) {
        super(n.split(!!options.vertical));
        n.splitAdd(this.id, first.id);
        n.splitAdd(this.id, second.id);
        if (options.ratio) this.ratio = options.ratio;
    }
    get ratio() { return n.getSplitRatio(this.id); }
    set ratio(value) { n.splitRatio(this.id, Number(value)); }
}

class MarkdownView extends Widget {
    constructor(markdown = "", options = {}) {
        super(n.markdownView(String(markdown)));
        if (options.onLink) this.onLink = options.onLink;
    }
    get markdown() { return n.getMarkdown(this.id); }
    set markdown(value) { n.setMarkdown(this.id, String(value)); }
    set onLink(fn) { n.on(this.id, "link", fn); }
    goTo(section) { return n.markdownGoTo(this.id, String(section).replace(/^#/, "")); }
}

// The object of its kind for a widget the host made from a description, without making another.
const kinds = {Label, Button, LineEdit, ProgressBar, TextEdit, CheckButton, ComboBox, Tabs, Split, MarkdownView, Grid};
function existing(kind, id) {
    const widget = Object.create((kinds[kind] ?? Widget).prototype);
    Object.defineProperty(widget, "id", {value: id});
    return widget;
}

class Ui {
    #number;
    #rootKind;
    constructor([number, rootId, rootKind, ...named]) {
        this.#number = number;
        this.#rootKind = rootKind;
        Object.defineProperty(this, "root", {value: existing(rootKind, rootId)});
        for (const [id, widgetId, kind] of named) {
            if (id === "root" || id === "put") {
                throw new Error(`an id cannot be ${id}, which a Ui has already`);
            }
            this[id] = existing(kind, widgetId);
        }
    }
    put(id, widget) {
        n.uiPut(this.#number, String(id), widget.id);
        this[id] = widget;
        return widget;
    }
    // The described Window, as a dialog's; static, so no id is taken by it.
    static dialogOf(ui) {
        if (ui.#rootKind !== "Window") {
            throw new Error(`oui.Dialog: the description starts with a ${ui.#rootKind}, not a Window: ` +
                "give oui.Dialog a title and ui.root");
        }
        return ui.root.id;
    }
    // Opens the window as the description's Window says; static, so no id is taken by it.
    static open(ui) {
        if (ui.#rootKind !== "Window") {
            throw new Error(`oui.window: the description starts with a ${ui.#rootKind}, not a Window: ` +
                "give oui.window a title and ui.root");
        }
        n.windowDescribed(ui.root.id);
    }
}

const fill = (widget) => ({fill: widget});

function box(vertical, children, options = {}) {
    const made = new Box(n.box(vertical, String(options.gap ?? "tight")));
    for (const child of children) {
        const filled = child !== null && typeof child === "object" && child.fill instanceof Widget;
        made.add(filled ? child.fill : child, {fill: filled});
    }
    return made;
}

const split = (first, second, options) => new Split(first, second, options);

const system = globalThis.__ouiSystem;
delete globalThis.__ouiSystem;

globalThis.oui = Object.freeze({
    ...system,
    args: n.args(),
    Widget, Label, Button, LineEdit, ProgressBar, TextEdit, CheckButton, ComboBox, List, Table, Tree, Tabs, Split,
    MarkdownView, Ui,
    Box, Grid, ScrollView, Panel, Dock, HSpacer, VSpacer, Comparison, FolderView, Terminal, Calculator, Dialog, Canvas,
    column: (children, options) => box(true, children, options),
    row: (children, options) => box(false, children, options),
    fill, split,
    window(title, content) {
        if (title instanceof Ui) {
            Ui.open(title);
            return;
        }
        n.window(String(title));
        if (content) n.windowContent(content.id);
    },
    ui: (description) => new Ui(n.ui(String(description))),
    uiFromFile: (path) => new Ui(n.uiFromFile(String(path))),
    shortcut: (keys, fn) => n.shortcut(String(keys), fn),
    setMenuBar: (menus) => n.menuBar(menus.map((menu) => ({title: String(menu.title), items: menuEntries(menu.items)}))),
    setTheme: (name) => n.theme(String(name)),
    theme: () => n.themeName(),
    setLanguage: (language) => n.setLanguage(String(language)),
    language: () => n.language(),
    settings: Object.freeze({
        value: (key, fallback = null) => {
            const kept = n.settingGet(String(key));
            return kept === null ? fallback : JSON.parse(kept);
        },
        setValue: (key, value) => n.settingSet(String(key), value === undefined ? null : JSON.stringify(value)),
        remove: (key) => n.settingSet(String(key), null),
    }),
    // The same as OUI Code's, for highlight's rules.
    words: (...lists) =>
        new RegExp("\\b(?:" + lists.flat().map((word) => word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|") + ")\\b"),
    setStatus: (text, right = "") => n.status(String(text), String(right)),
    quit: () => n.quit(),
    exit: (code = 0) => n.exit(Number(code)),
});
}
