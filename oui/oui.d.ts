// What OUI's scripts can use, besides JavaScript itself. tools/extract_api.js makes it from the comments marked
// "api" beside the code that defines each name: change those, and run it again, rather than this file.

/**
 * Stops what setInterval started.
 * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
 */
declare function clearInterval(id: number): void;

/**
 * Stops what setTimeout started, if it is still to come.
 * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
 */
declare function clearTimeout(id: number): void;

/**
 * Calls fn with args every ms milliseconds, until clearInterval with what it gives.
 * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
 */
declare function setInterval(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number;

/**
 * Calls fn with args once, ms milliseconds from now; clearTimeout with what it gives stops it.
 * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
 */
declare function setTimeout(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number;

declare namespace console {
    /**
     * Writes the values on a line, apart by spaces: to the terminal in oui_run, to the output tab in OUI Code.
     * In: Every script.
     */
    function log(...values: any[]): void;
}

declare namespace oui {
    /**
     * The arguments after the script's name.
     * In: oui_run.
     */
    const args: string[];

    /**
     * What the script keeps from one run to the next, by names of its choosing: any value JSON holds. In
     * <script>.ini, in the user's configuration folder for oui_run, ~/.config/oui_run on Linux; scripts with the same
     * name share it.
     * In: oui_run.
     */
    const settings: {value(key: string, fallback?: any): any, setValue(key: string, value: any): void, remove(key: string): void};

    /**
     * Asks to pick one of options, and gives its label; null when cancelled. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function choose(title: string, options: (string | {label: string, detail?: string})[]): Promise<string | null>;

    /**
     * As oui.openFile, for a folder.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function chooseFolder(options?: {title?: string, folder?: string}): Promise<string | null>;

    /**
     * The children one under the other; one given through oui.fill takes the room left.
     * In: oui_run.
     */
    function column(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box;

    /**
     * Asks yes or no. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function confirm(question: string): Promise<boolean>;

    /**
     * Puts text on the clipboard, the system's where there is one. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function copy(text: string): void;

    /**
     * Whether there is a file or a folder at path.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function exists(path: string): boolean;

    /**
     * Ends the program there and then, with code, 0 unless given: nothing after it runs, not even a catch or a finally.
     * In: oui_run.
     */
    function exit(code?: number): void;

    /**
     * A widget that takes the room a column or a row leaves.
     * In: oui_run.
     */
    function fill(widget: oui.Widget): {fill: oui.Widget};

    /**
     * The language OUI's own texts are in, as "en" or "es".
     * In: oui_run.
     */
    function language(): string;

    /**
     * The names in a folder, in order; a folder's ends in "/".
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function listDir(path: string): string[];

    /**
     * Tells something, in a window when there is one and on the terminal when not. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function message(title: string, text?: string): Promise<void>;

    /**
     * Hands a web address or a file to what the system opens it with, as a browser, and does not wait for it; false
     * when the system could not be asked. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function openExternally(target: string): boolean;

    /**
     * Asks for a file to open, in the file dialog: from folder, the current one unless given; filters as {name:
     * "Scripts", patterns: "*.js;*.ts"}. Gives its path, with / between folders on every system; null when cancelled.
     * Without a window, its path is asked on the terminal. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function openFile(options?: {title?: string, folder?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>;

    /**
     * What the clipboard holds, as text.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function paste(): string;

    /**
     * Asks for a line of text, starting with text; null when cancelled. For a password, what is typed shows as dots, or not
     * at all when asked on the terminal. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function prompt(title: string, label?: string, text?: string, options?: {password?: boolean}): Promise<string | null>;

    /**
     * Ends the program once what it is doing now is done.
     * In: oui_run.
     */
    function quit(): void;

    /**
     * The file's text, or null when it cannot be read.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function readFile(path: string): string | null;

    /**
     * The children side by side; one given through oui.fill takes the room left.
     * In: oui_run.
     */
    function row(children: (oui.Widget | {fill: oui.Widget})[], options?: {gap?: "none" | "tight" | "normal" | "loose"}): oui.Box;

    /**
     * Runs a shell command and gives its exit code. Its output goes to the terminal, or to output as it comes; input
     * is what it reads; cwd the folder it runs in. stop() on the promise stops it. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function run(command: string, options?: {cwd?: string, input?: string, output?: (text: string) => void}): Promise<number> & {stop(): void};

    /**
     * As oui.openFile, for a file to save as, name offered: it asks before one that is there is replaced.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function saveFile(options?: {title?: string, folder?: string, name?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>;

    /**
     * The language OUI's own texts are in, as a dialog's buttons and the file dialog: "en" or "es", the system's until
     * told. A script written in one language says it first, before its window, so all it shows is in that one.
     * In: oui_run.
     */
    function setLanguage(language: string): void;

    /**
     * The window's menus along its top, each a title and its entries: an entry a title, the keys that run it too, as
     * "Ctrl+O", and what it runs; "-" a line between entries. & marks the letter Alt goes with. Before or after
     * oui.window; asked again, the new menus take the old ones' place.
     * In: oui_run.
     */
    function setMenuBar(menus: {title: string, items: ({title: string, keys?: string, run: () => void} | "-")[]}[]): void;

    /**
     * What the line along the bottom of the window says, and what it says at its right end; the line shows from the
     * first time.
     * In: oui_run.
     */
    function setStatus(text: string, right?: string): void;

    /**
     * The look: "turbo", "night", "light", "monochrome", "amber", "phosphor" or "commander" in a terminal, and "classic"
     * in a window too.
     * In: oui_run.
     */
    function setTheme(name: string): void;

    /**
     * Calls fn when keys are pressed in the window, written as "Ctrl+S" or "Alt+Shift+F2".
     * In: oui_run.
     */
    function shortcut(keys: string, fn: () => void): void;

    /**
     * new oui.Split, as a function beside oui.column and oui.row.
     * In: oui_run.
     */
    function split(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number}): oui.Split;

    /**
     * The look in use, by its name: the one chosen, or "classic" in a desktop window and "turbo" in a terminal.
     * In: oui_run.
     */
    function theme(): string;

    /**
     * Builds the widgets a description says, as a .oui file of OUI's writes them: a widget a line, what goes in it
     * indented under it, and its properties after it.
     * In: oui_run.
     */
    function ui(description: string): oui.Ui;

    /**
     * oui.ui of the file at path, found from the script's folder when it is not whole, wherever the script is run from.
     * In: oui_run.
     */
    function uiFromFile(path: string): oui.Ui;

    /**
     * Opens the window, titled title, with content in it: any widget, as a column of others.
     * In: oui_run.
     */
    function window(title: string, content?: oui.Widget): void;

    /**
     * Opens the window a description's Window says: its title, and what it holds.
     * In: oui_run.
     */
    function window(ui: oui.Ui): void;

    /**
     * A regular expression for any of the words, whole words only, as keyword rules want: oui.words(["if", "else"]).
     * In: OUI Code's language and command scripts, and oui_run.
     */
    function words(...lists: string[][]): RegExp;

    /**
     * Writes text as the file, whether it was there or not; false when it cannot. Not in an interactive script.
     * In: oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.
     */
    function writeFile(path: string, text: string): boolean;

    /**
     * What oui.column and oui.row make.
     * In: oui_run.
     */
    class Box extends Widget {
        /**
         * Puts widget after the others; with fill it takes the room they leave.
         * In: oui_run.
         */
        add(widget: oui.Widget, options?: {fill?: boolean}): this;
    }

    /**
     * A button, which calls its onClick when pressed.
     * In: oui_run.
     */
    class Button extends Widget {
        /**
         * What it says, & marking the letter Alt presses it with.
         * In: oui_run.
         */
        text: string;

        /**
         * A default button is the one Enter presses in a dialog.
         * In: oui_run.
         */
        constructor(text: string, onClick?: () => void, options?: {default?: boolean});

        /**
         * What pressing it does.
         * In: oui_run.
         */
        onClick: () => void;
    }

    /**
     * OUI's calculator: what is written on its line worked out, as oui_calc does.
     * In: oui_run.
     */
    class Calculator extends Widget {
        constructor(options?: {onExit?: () => void});
    }

    /**
     * What a script draws on, in cells, as text and filled areas: what is drawn stays until clear(). Of a size of its
     * own, or of all the room it has.
     * In: oui_run.
     */
    class Canvas extends Widget {
        constructor(options?: {width?: number, height?: number, onMouse?: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void, onResize?: (width: number, height: number) => void});

        /**
         * How wide it is now, in cells; height, how tall.
         * In: oui_run.
         */
        width: number;

        height: number;

        /**
         * Forgets what was drawn, its background a colour as widgets take them, the theme's unless given.
         * In: oui_run.
         */
        clear(background?: string): this;

        /**
         * Writes text from the cell at x, y, counted from 0 at its top left corner.
         * In: oui_run.
         */
        text(x: number, y: number, text: string, style?: {color?: string, background?: string, bold?: boolean, underline?: boolean}): this;

        /**
         * Fills cells with a colour.
         * In: oui_run.
         */
        fill(x: number, y: number, width: number, height: number, background: string): this;

        /**
         * Called with what the pointer does over it: the cell under it, from the canvas's corner, and what it did. A press
         * with clicks 2 is a double click; wheel is how far the wheel turned.
         * In: oui_run.
         */
        onMouse: (mouse: {x: number, y: number, kind: "press" | "release" | "drag" | "move" | "wheel", right: boolean, clicks: number, wheel: number, shift: boolean, control: boolean, alt: boolean}) => void;

        /**
         * Called when its size changes, the first time too: where to draw it again.
         * In: oui_run.
         */
        onResize: (width: number, height: number) => void;
    }

    /**
     * A box to tick, with its text beside it.
     * In: oui_run.
     */
    class CheckButton extends Widget {
        constructor(text: string, options?: {checked?: boolean, onToggle?: (checked: boolean) => void});

        /**
         * Whether it is ticked.
         * In: oui_run.
         */
        checked: boolean;

        /**
         * Called when it is ticked or unticked.
         * In: oui_run.
         */
        onToggle: (checked: boolean) => void;
    }

    /**
     * One of a few options, the others in a list that opens.
     * In: oui_run.
     */
    class ComboBox extends Widget {
        constructor(items?: string[], options?: {onSelect?: (index: number) => void});

        /**
         * Its options.
         * In: oui_run.
         */
        items: string[];

        /**
         * Which option is chosen, from 0.
         * In: oui_run.
         */
        selected: number;

        /**
         * Called when another option is chosen.
         * In: oui_run.
         */
        onSelect: (index: number) => void;
    }

    /**
     * Two texts side by side, what differs between them marked, as OUI Code compares files.
     * In: oui_run.
     */
    class Comparison extends Widget {
        constructor(options?: {left?: string, right?: string, leftTitle?: string, rightTitle?: string});

        /**
         * Shows the two texts, under their titles.
         * In: oui_run.
         */
        compare(left: string, right: string, leftTitle?: string, rightTitle?: string): this;

        /**
         * How many places differ.
         * In: oui_run.
         */
        differences: number;

        /**
         * Goes to the next difference; previous() to the one before.
         * In: oui_run.
         */
        next(): this;
    }

    /**
     * A window of the script's own over the main one, which waits on it: what it holds, under its title. Escape or its
     * close button cancels it. It opens again as often as it is shown, with its widgets as they were left.
     * In: oui_run.
     */
    class Dialog {
        /**
         * width and height in cells, the least it opens at; resizable lets the mouse make it bigger.
         * In: oui_run.
         */
        constructor(title: string, content: oui.Widget, options?: {width?: number, height?: number, resizable?: boolean});

        /**
         * The dialog a description's Window says: its title, its size, and what it holds.
         * In: oui_run.
         */
        constructor(ui: oui.Ui);

        /**
         * Opens it and waits for it to close: what close was given, or null when it was cancelled.
         * In: oui_run.
         */
        show(): Promise<any>;

        /**
         * Closes it, show giving back value, true unless given.
         * In: oui_run.
         */
        close(value?: any): void;
    }

    /**
     * Widgets against its edges, as a window's bars and side panels, and one in the room they leave.
     * In: oui_run.
     */
    class Dock extends Widget {
        constructor();

        /**
         * Each takes its strip of the room the ones before left; size in cells, its own unless given; a side strip with
         * resizable is made wider by the mouse, keeping that many cells at least.
         * In: oui_run.
         */
        add(side: "top" | "bottom" | "left" | "right" | "fill", widget: oui.Widget, options?: {size?: number, resizable?: number}): this;
    }

    /**
     * A folder's files and folders, as OUI Code's explorer: Enter goes into a folder, Backspace up.
     * In: oui_run.
     */
    class FolderView extends Widget {
        constructor(folder?: string, options?: {onActivate?: (path: string) => void});

        /**
         * The folder shown, whole.
         * In: oui_run.
         */
        folder: string;

        /**
         * The path of what is chosen in it.
         * In: oui_run.
         */
        selected: string | null;

        /**
         * Called with a file chosen with Enter or a double click.
         * In: oui_run.
         */
        onActivate: (path: string) => void;
    }

    /**
     * Widgets in columns and rows, a row at a time, as a description's Grid.
     * In: oui_run.
     */
    class Grid extends Widget {
        /**
         * A length is a number of cells, as wide as what is in it, the rest, a share of the rest, or "40%".
         * In: oui_run.
         */
        constructor(options?: {columns?: (number | "content" | "fill" | "flex" | string)[], rows?: (number | "content" | "fill" | "flex" | string)[], gap?: "none" | "tight" | "normal" | "loose"});

        /**
         * Puts the widgets in, after what is there.
         * In: oui_run.
         */
        add(...widgets: oui.Widget[]): this;
    }

    /**
     * Room across, of a width in cells.
     * In: oui_run.
     */
    class HSpacer extends Widget {
        constructor(width: number);
    }

    /**
     * A line of text, or a paragraph when it wraps.
     * In: oui_run.
     */
    class Label extends Widget {
        constructor(text?: string, options?: {bold?: boolean, wrap?: boolean});

        /**
         * What it says.
         * In: oui_run.
         */
        text: string;
    }

    /**
     * A field of one line.
     * In: oui_run.
     */
    class LineEdit extends Widget {
        constructor(options?: {text?: string, placeholder?: string, password?: boolean, onChange?: (text: string) => void, onSubmit?: (text: string) => void});

        /**
         * What is written in it.
         * In: oui_run.
         */
        text: string;

        /**
         * Called at every change.
         * In: oui_run.
         */
        onChange: (text: string) => void;

        /**
         * Called when Enter is pressed in it.
         * In: oui_run.
         */
        onSubmit: (text: string) => void;

        /**
         * What it shows, quieter, while it is empty.
         * In: oui_run.
         */
        placeholder: string;

        /**
         * Whether it is for a password: a dot shows for each character, copying and cutting take nothing, and Control with an
         * arrow goes to an end rather than to a word's. Its text is what was typed.
         * In: oui_run.
         */
        password: boolean;

        /**
         * What is selected, from start to end, as places in its text; the same twice where the cursor is.
         * In: oui_run.
         */
        selection: [number, number];

        /**
         * Selects from start to end, the cursor at end; with one place, only puts the cursor there.
         * In: oui_run.
         */
        select(start: number, end?: number): this;

        selectAll(): this;

        selectedText: string;

        /**
         * Writes text in place of the selection, as typing would.
         * In: oui_run.
         */
        insert(text: string): this;
    }

    /**
     * A list to pick from, with the keyboard or the mouse.
     * In: oui_run.
     */
    class List extends Widget {
        /**
         * With checks, each item has a box before it, which a click on it, or Space on the item chosen, ticks.
         * In: oui_run.
         */
        constructor(items?: string[], options?: {checks?: boolean, onSelect?: (index: number) => void, onActivate?: (index: number) => void, onToggle?: (index: number, checked: boolean) => void});

        /**
         * What it lists.
         * In: oui_run.
         */
        items: string[];

        /**
         * Which item is selected, from 0.
         * In: oui_run.
         */
        selected: number;

        /**
         * Called when another item is selected.
         * In: oui_run.
         */
        onSelect: (index: number) => void;

        /**
         * Called when an item is chosen, with Enter or a double click.
         * In: oui_run.
         */
        onActivate: (index: number) => void;

        /**
         * With checks, whether each item's box is ticked; setting items unticks them all.
         * In: oui_run.
         */
        checked: boolean[];

        /**
         * With checks, called when a box is ticked or unticked.
         * In: oui_run.
         */
        onToggle: (index: number, checked: boolean) => void;
    }

    /**
     * Markdown to read: headings, lists, tables, code.
     * In: oui_run.
     */
    class MarkdownView extends Widget {
        constructor(markdown?: string, options?: {onLink?: (target: string) => void});

        /**
         * What it shows.
         * In: oui_run.
         */
        markdown: string;

        /**
         * Called when a link that leads out of the text is followed, with where it leads, as written: a web address to hand
         * to oui.openExternally, or a file to show. One to a section of the text, "#its-heading", the view goes to itself.
         * In: oui_run.
         */
        onLink: (target: string) => void;

        /**
         * Shows the heading a link to its section names at the top, "installing" for "## Installing"; false when there is
         * none.
         * In: oui_run.
         */
        goTo(section: string): boolean;
    }

    /**
     * What it holds, with room round it, on the panels' colour.
     * In: oui_run.
     */
    class Panel extends Widget {
        /**
         * padding in cells: one for every side, or top, right, bottom and left.
         * In: oui_run.
         */
        constructor(content: oui.Widget, options?: {padding?: number | [number, number, number, number]});
    }

    /**
     * How far something has gone, as a bar that fills from the left, with a text after it, as "4 of 8".
     * In: oui_run.
     */
    class ProgressBar extends Widget {
        constructor(options?: {value?: number, text?: string});

        /**
         * How much is done, from 0, nothing, to 1, all of it; kept within them.
         * In: oui_run.
         */
        value: number;

        /**
         * What it says after the bar; nothing when empty.
         * In: oui_run.
         */
        text: string;
    }

    /**
     * What it holds, scrolled when it is taller than the room it has.
     * In: oui_run.
     */
    class ScrollView extends Widget {
        constructor(content: oui.Widget);
    }

    /**
     * Two widgets with a line between them that the mouse drags: side by side, or one over the other when vertical.
     * In: oui_run.
     */
    class Split extends Widget {
        constructor(first: oui.Widget, second: oui.Widget, options?: {vertical?: boolean, ratio?: number});

        /**
         * The first one's share, from 0 to 1: half unless given.
         * In: oui_run.
         */
        ratio: number;
    }

    /**
     * Rows of cells under headings. A click on a heading asks for the rows in its order, ascending, then descending,
     * then as they were: onSort hears it, and the script puts its rows in that order and sets them again.
     * In: oui_run.
     */
    class Table extends Widget {
        /**
         * The headings, and the widths of the columns in cells: every one but the last, which takes the rest.
         * In: oui_run.
         */
        constructor(headings: string[], options?: {widths?: number[], rows?: (string | number)[][], onSelect?: (index: number) => void, onActivate?: (index: number) => void, onSort?: (column: number, order: "ascending" | "descending" | "none") => void});

        /**
         * What it shows, a row an array of cells. Set again, the row chosen and where it was scrolled to stay.
         * In: oui_run.
         */
        rows: (string | number)[][];

        /**
         * Which row is chosen, from 0.
         * In: oui_run.
         */
        selected: number;

        /**
         * Called when another row is chosen.
         * In: oui_run.
         */
        onSelect: (index: number) => void;

        /**
         * Called when a row is chosen with Enter or a double click.
         * In: oui_run.
         */
        onActivate: (index: number) => void;

        /**
         * Called when a heading is clicked, with the order it asks for.
         * In: oui_run.
         */
        onSort: (column: number, order: "ascending" | "descending" | "none") => void;

        /**
         * Marks the heading of the column the rows are in the order of, as a click on it would.
         * In: oui_run.
         */
        sortBy(column: number, order: "ascending" | "descending" | "none"): this;

        /**
         * The columns' widths, in cells.
         * In: oui_run.
         */
        widths: number[];

        /**
         * The column that takes the room the others leave: the last unless set; null for none.
         * In: oui_run.
         */
        stretch: number | null;

        /**
         * Whether dragging the edge between two headings makes a column wider or narrower.
         * In: oui_run.
         */
        resizable: boolean;

        /**
         * Whether its headings show.
         * In: oui_run.
         */
        headings: boolean;

        /**
         * Scrolls so the row is in sight.
         * In: oui_run.
         */
        show(row: number): this;

        /**
         * Called when a column was made wider or narrower with the mouse.
         * In: oui_run.
         */
        onResize: (column: number, width: number) => void;
    }

    /**
     * Pages one at a time, each with its title on a tab over them.
     * In: oui_run.
     */
    class Tabs extends Widget {
        constructor(options?: {fixed?: boolean, onSelect?: (index: number) => void});

        /**
         * Adds a page after the others.
         * In: oui_run.
         */
        add(title: string, widget: oui.Widget): this;

        /**
         * How many pages it has.
         * In: oui_run.
         */
        count: number;

        /**
         * The page on show, from 0.
         * In: oui_run.
         */
        selected: number;

        /**
         * Called when another page shows.
         * In: oui_run.
         */
        onSelect: (index: number) => void;

        /**
         * Its pages, in order.
         * In: oui_run.
         */
        pages: oui.Widget[];

        /**
         * The title of a page; setTitle(index, title) changes it.
         * In: oui_run.
         */
        title(index: number): string;

        setTitle(index: number, title: string): this;

        /**
         * Takes a page out, and gives it, to go elsewhere or nowhere.
         * In: oui_run.
         */
        remove(index: number): oui.Widget;

        /**
         * Moves a page among the others; the page on show stays on show.
         * In: oui_run.
         */
        move(from: number, to: number): this;

        /**
         * Which pages ask for attention, their close mark in the warning colour, as documents not saved.
         * In: oui_run.
         */
        marked: boolean[];

        /**
         * Called when a page's close mark is pressed: the page goes unless this is set, and then only if it says so with
         * remove(index).
         * In: oui_run.
         */
        onClose: (index: number) => void;
    }

    /**
     * A program running in a terminal of its own, the user's shell unless told another: what is typed goes to it.
     * In: oui_run.
     */
    class Terminal extends Widget {
        constructor(options?: {command?: string[], folder?: string, onFinished?: (code: number) => void, onTitle?: (title: string) => void});

        /**
         * Sends text as pasted.
         * In: oui_run.
         */
        paste(text: string): this;

        /**
         * Whether its program is still running.
         * In: oui_run.
         */
        running: boolean;
    }

    /**
     * A text of many lines, to write or only to show.
     * In: oui_run.
     */
    class TextEdit extends Widget {
        constructor(options?: {text?: string, readOnly?: boolean, wrap?: boolean, lineNumbers?: boolean, onChange?: (text: string) => void});

        /**
         * All of its text.
         * In: oui_run.
         */
        text: string;

        /**
         * Whether it can only be read.
         * In: oui_run.
         */
        readOnly: boolean;

        /**
         * Adds text at its end, as output that comes.
         * In: oui_run.
         */
        append(text: string): void;

        /**
         * Called at every change.
         * In: oui_run.
         */
        onChange: (text: string) => void;

        /**
         * Whether lines too long for it go on below rather than off its side.
         * In: oui_run.
         */
        wrap: boolean;

        lineNumbers: boolean;

        /**
         * Where the cursor is, as a place in its text; set, the cursor goes there.
         * In: oui_run.
         */
        cursor: number;

        /**
         * The cursor's line, from 1; column is its place in the line, from 1.
         * In: oui_run.
         */
        line: number;

        column: number;

        /**
         * What is selected, from start to end, as places in its text; the same twice where the cursor is.
         * In: oui_run.
         */
        selection: [number, number];

        /**
         * Selects from start to end, the cursor at end; with one place, only puts the cursor there.
         * In: oui_run.
         */
        select(start: number, end?: number): this;

        selectAll(): this;

        selectedText: string;

        /**
         * Writes text in place of the selection, as typing would: one step to undo, and nothing when it is read only.
         * In: oui_run.
         */
        insert(text: string): this;

        /**
         * Whether it changed since it was set false, as a document saved.
         * In: oui_run.
         */
        modified: boolean;

        /**
         * Selects the next place with text from the cursor, or the one before it backward, going round past the end;
         * false when there is none. Case matters unless matchCase is false; regex takes text as a regular expression.
         * In: oui_run.
         */
        find(text: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean, backward?: boolean}): boolean;

        /**
         * Replaces the selection when it is text, and selects the next one; false when there is none.
         * In: oui_run.
         */
        replace(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): boolean;

        /**
         * Replaces every one, as one step to undo, and gives how many.
         * In: oui_run.
         */
        replaceAll(text: string, replacement: string, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number;

        /**
         * Marks every place with text, as a find bar does, until called with null; gives how many there are.
         * In: oui_run.
         */
        showMatches(text: string | null, options?: {matchCase?: boolean, wholeWord?: boolean, regex?: boolean}): number;

        /**
         * Puts the cursor at the start of the line, from 1; false when there is no such line.
         * In: oui_run.
         */
        goToLine(line: number): boolean;

        /**
         * Undoes the last change; false when there is none. redo() does it again.
         * In: oui_run.
         */
        undo(): boolean;

        redo(): boolean;

        /**
         * The lines the selection touches, or the cursor's: copied below them. moveLines("up" or "down") moves them,
         * deleteLines() deletes them, toggleComment(prefix) comments them out with prefix, or back.
         * In: oui_run.
         */
        duplicateLines(): boolean;

        moveLines(direction: "up" | "down"): boolean;

        deleteLines(): boolean;

        toggleComment(prefix: string): boolean;

        /**
         * How many spaces Tab writes, with useSpaces; a tab otherwise.
         * In: oui_run.
         */
        tabSpaces: number;

        useSpaces: boolean;

        /**
         * Whether Enter starts the new line as indented as the one before.
         * In: oui_run.
         */
        autoIndent: boolean;

        /**
         * Whether the other places with the selected word are marked.
         * In: oui_run.
         */
        occurrences: boolean;

        /**
         * Colours the text by rules, written as an OUI Code language's states: {start: [{match: oui.words(["if"]), kind:
         * "keyword"}, {match: /"/, push: "string"}], string: {kind: "string", rules: [{match: /"/, pop: true}]}}. null
         * leaves it in one colour.
         * In: oui_run.
         */
        highlight: {[state: string]: object[] | {kind: string, rules: object[]}} | null;

        /**
         * Called when the cursor or the selection moves.
         * In: oui_run.
         */
        onCursor: () => void;
    }

    /**
     * Nodes in a tree, each with those under it, which open and close: a node is where it is, its places from the top,
     * [1, 0] the first under the second.
     * In: oui_run.
     */
    class Tree extends Widget {
        constructor(items?: {text: string, children?: object[], expanded?: boolean}[], options?: {onSelect?: (path: number[]) => void, onActivate?: (path: number[]) => void});

        /**
         * What it shows: each node its text, the nodes under it as nodes too, and whether it starts open.
         * In: oui_run.
         */
        items: {text: string, children?: object[], expanded?: boolean}[];

        /**
         * Where the node chosen is; set, the nodes above it open.
         * In: oui_run.
         */
        selected: number[] | null;

        /**
         * Called when another node is chosen.
         * In: oui_run.
         */
        onSelect: (path: number[]) => void;

        /**
         * Called when a node is chosen with Enter or a double click.
         * In: oui_run.
         */
        onActivate: (path: number[]) => void;
    }

    /**
     * The widgets of a description: each with an id by its id, as the object of its kind, ui.go a oui.Button. An id
     * cannot be root or put.
     * In: oui_run.
     */
    class Ui {
        /**
         * The widget the description starts with, to put in a window or anywhere else.
         * In: oui_run.
         */
        root: oui.Widget;

        /**
         * Puts widget where the description has the Slot with the id, which then names it: a widget a description cannot
         * write, as a oui.List.
         * In: oui_run.
         */
        put(id: string, widget: oui.Widget): oui.Widget;
    }

    /**
     * Room down, of a height in cells.
     * In: oui_run.
     */
    class VSpacer extends Widget {
        constructor(height: number);
    }

    /**
     * What every widget is: a label, a button, a field, a column...
     * In: oui_run.
     */
    class Widget {
        /**
         * What the host knows it by.
         * In: oui_run.
         */
        id: number;

        /**
         * Gives it the keyboard; one that cannot have it, as a column or a calculator, gives it to the first of its widgets
         * that can.
         * In: oui_run.
         */
        focus(): this;

        /**
         * Whether it shows; one that does not takes no room either.
         * In: oui_run.
         */
        visible: boolean;

        /**
         * Whether it answers: a disabled widget shows so, and takes no click, no key and no focus. What a disabled one holds
         * is disabled with it.
         * In: oui_run.
         */
        enabled: boolean;

        /**
         * What shows by the pointer resting on it.
         * In: oui_run.
         */
        tooltip: string;

        /**
         * The colour behind it, one of the theme's by its name, "table_background", or a fixed one, "#20a0ff". Labels,
         * fields, buttons, boxes to tick, Markdown views and trees have one.
         * In: oui_run.
         */
        background: string;

        /**
         * The colour of its text, as background takes one. Labels and Markdown views have one.
         * In: oui_run.
         */
        color: string;

        /**
         * Takes it out of what it is in, to put it in another or nowhere; a page of tabs, or a side of a split, stays.
         * In: oui_run.
         */
        remove(): this;

        /**
         * The entries a right click on it opens, as a menu's, where the pointer is.
         * In: oui_run.
         */
        contextMenu: ({title: string, keys?: string, run: () => void} | "-")[];

        /**
         * Hears a key before the widget does, while it or what it holds has the focus: the key as shortcuts write it,
         * "Ctrl+S" or "Enter", and the text it types, if any; true keeps it from going further.
         * In: oui_run.
         */
        onKey: (key: string, text: string) => boolean | void;
    }

    namespace commands {
        /**
         * Adds a text command, in the palette and Alt+X, on keys when given: run gets the selection, or the whole document
         * when nothing is selected, and gives back what goes in its place, or nothing to leave it. It shows in Tools > Text
         * Commands, or where menuPath puts it, as a path of files: "edit" in the Edit menu, "edit/Case" in a submenu Case of
         * it, made when there is none. The first name is the menu's, file, edit, view, tools or help, whatever language
         * OUI Code is in; the rest are submenus as they show, OUI Code's own found by their name in English or as translated.
         * Like the title, it can be one per language: {en: "edit/Case", es: "edit/Mayúsculas"}.
         * In: OUI Code's language and command scripts.
         */
        function register(command: {id: string, title: string | {[language: string]: string}, keys?: string, menuPath?: string | {[language: string]: string}, run: (text: string) => string | undefined}): void;
    }

    namespace languages {
        /**
         * Adds a language, or replaces one with the same id: how files of its extensions and names are coloured, from the
         * rules of each state, how a line is commented, which brackets pair, and what it suggests as one writes: words,
         * as its keywords, or pieces of text with a name, where "$0" marks where the cursor goes.
         * In: OUI Code's language and command scripts.
         */
        function register(language: {id: string, name?: string, extensions?: string[], files?: string[], comment?: string, brackets?: string[], states: {[state: string]: object[]}, suggestions?: (string | {label: string, detail?: string, text?: string})[]}): void;
    }
}
