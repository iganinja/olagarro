// OUI Code as a person uses it, in a terminal: run by oui_run, tests/oui_code/end_to_end.js PATH_TO_OUI_CODE [CASE],
// on what tests/lib/terminal_session.js does: OUI Code copied into a folder of its own, run in a pseudo terminal 100
// columns by 24 rows, keys pressed, and the screen it leaves looked at.
import {expect, place, run, session} from "../lib/terminal_session.js";

const [ouiCode, only] = oui.args;
if (!ouiCode) {
    console.log("usage: oui_run tests/oui_code/end_to_end.js PATH_TO_OUI_CODE [CASE]");
    oui.exit(2);
}

// The keys as a terminal sends them.
const keys = {
    f1: "\x1bOP",
    f2: "\x1bOQ",
    f7: "\x1b[18~",
    f9: "\x1b[20~",
    quit: "\x11",
    enter: "\r",
    tools: "\x1bt",
    previousTab: "\x1b[5;5~",
    nextTab: "\x1b[6;5~",
};

const cases = [];
const test = (name, run) => cases.push({name, run});

const files = {"left.txt": "one\ngone\nthree\nfour\n", "right.txt": "one\nthree\nfour\nnew\n"};

test("--diff opens the comparison alone, the removed line in red", async () => {
    const shown = await session(ouiCode, ["--diff", "left.txt", "right.txt"], files, []);
    expect("the tab", shown.find("left.txt vs right.txt") !== null, true);
    expect("an untitled document beside it", shown.find("Untitled"), null);
    // White on red: what Turbo's colours make of a line removed.
    expect("the removed line", shown.find("gone")?.colours, "97;41");
    expect("the count", shown.find("2 differences") !== null, true);
});

test("F7 goes to a difference straight away", async () => {
    const shown = await session(ouiCode, ["--diff", "left.txt", "right.txt"], files, [keys.f7, keys.f7]);
    expect("the second difference", shown.find("Difference 2 of 2") !== null, true);
});

test("Enter goes to the line in its file, on the other side when the chosen one has none", async () => {
    const shown = await session(ouiCode, ["--diff", "left.txt", "right.txt"], files, [keys.f7, keys.enter]);
    // The line removed, 2 of left.txt: the right side, chosen unless clicked, has none there.
    expect("the file at its line", shown.find("Ln 2, Col 1") !== null, true);
    expect("its tab", shown.find(" left.txt ×") !== null, true);
});

test("Compare with Saved shows what the document has that its file does not", async () => {
    // Alt+T opens the Tools menu, and S is the letter of Compare with Saved there.
    // Deep enough that its whole path does not fit over a side.
    const deep = "a_folder_with_a_rather_long_name/and_another_one_inside_it/left.txt";
    const shown = await session(ouiCode, [deep], {[deep]: files["left.txt"]}, ["X", keys.tools, "s"]);
    expect("the tab", shown.find("left.txt (saved) vs left.txt") !== null, true);
    // Over each side the file's whole path, cut from its start where it does not fit, so its name shows.
    expect("the saved side's title", shown.find("/left.txt (saved)")?.row, 2);
    // Too little in common to say what changed within: the saved line removed whole, the edited one added.
    // Each after its line's number, 1.
    expect("the saved line", shown.find("1 one", 2)?.colours, "97;41");
    expect("the edited line", shown.find("1 Xone", 2)?.colours, "97;42");
});

test("A comparison follows the document as it is edited", async () => {
    // Compare with Saved, back to the document to write some more, and to the comparison again.
    const shown = await session(ouiCode, ["left.txt"], files, ["X", keys.tools, "s", keys.previousTab, "Y", keys.nextTab]);
    expect("the edited line, as it is now", shown.find("1 XYone", 2)?.colours, "97;42");
});

test("Tools > Calculator opens a calculator in a tab, the line ready to write on", async () => {
    // L is the letter of Calculator in the Tools menu.
    const shown = await session(ouiCode, [], {}, [keys.tools, "l", "interes = 800*0.03\r", "interes + 800\r"]);
    expect("the tab", shown.find("Calculator ×") !== null, true);
    expect("the name kept", shown.find("interes = 24") !== null, true);
    expect("the name used", shown.find("= 824") !== null, true);
});

test("F2 in the calculator's tab opens its options", async () => {
    const shown = await session(ouiCode, [], {}, [keys.tools, "l", keys.f2, " ", keys.enter, "sin(30)\r"]);
    expect("degrees", shown.find("DEG") !== null, true);
    expect("the sine in degrees", shown.find("= 0.5") !== null, true);
});

test("exit in the calculator's tab closes the tab", async () => {
    const shown = await session(ouiCode, [], {}, [keys.tools, "l", "exit\r"]);
    expect("the tab", shown.find("Calculator ×"), null);
});

test("F1 in the calculator's tab opens the help at the calculator", async () => {
    const shown = await session(ouiCode, [], {}, [keys.tools, "l", keys.f1]);
    expect("its part", shown.find("Write an expression as you would on paper") !== null, true);
});

test("Files given on the command line open over the session left, every one of them", async () => {
    const here = await place(ouiCode, {...files, "notes.txt": "a note\n"});
    await here.run(["left.txt"], [keys.quit]);
    const shown = await here.run(["right.txt", "notes.txt"], [""]);
    await here.remove();
    const tabs = shown.row(1);
    for (const title of ["left.txt", "right.txt", "notes.txt"]) {
        expect(title, tabs.includes(title), true);
    }
    expect("the last one given in front", shown.find("a note") !== null, true);
});

test("A file given that is not there yet opens empty under its name, and Ctrl+S makes it", async () => {
    const here = await place(ouiCode, files);
    const shown = await here.run(["new.txt"], ["hola ñ", "\x13", ""]);
    const written = oui.readFile(`${here.folder}/new.txt`);
    await here.remove();
    expect("its tab", shown.row(1).includes("new.txt"), true);
    expect("the file made", written, "hola ñ");
});

test("A Markdown preview follows a link to another file, goes back with Alt+Left, and Ctrl+Shift+E edits it", async () => {
    const pages = {
        "guide.md": "# Guide\n\nSee [the parts](parts.md#second) of it.\n",
        "parts.md": "# Parts\n\n## First\n\nThe first part.\n\n## Second\n\nThe second part.\n",
    };
    // F9 previews the document; Tab goes to the link and Enter follows it.
    const follow = [keys.f9, "\t", keys.enter, ""];
    const followed = await session(ouiCode, ["guide.md"], pages, follow);
    expect("the file it leads to", followed.find("The second part.") !== null, true);
    expect("its tab", followed.row(1).includes("Preview: parts.md"), true);
    const titles = (shown) => shown.row(1).split("×").map((title) => title.trim());
    expect("not opened to edit", titles(followed).includes("parts.md"), false);
    const back = await session(ouiCode, ["guide.md"], pages, [...follow, "\x1b[1;3D", ""]);
    expect("back where it was", back.find("See the parts of it.") !== null, true);
    const edited = await session(ouiCode, ["guide.md"], pages, [...follow, "\x1b[69;6u", ""]);
    expect("opened to edit", edited.find("## Second") !== null, true);
    expect("its own tab", titles(edited).includes("parts.md"), true);
});

test("F1 in a comparison opens the help at comparing files", async () => {
    const shown = await session(ouiCode, ["--diff", "left.txt", "right.txt"], files, [keys.f1]);
    expect("its part", shown.find("compares the document you are editing") !== null, true);
});

test("The tabs come back after quitting, in their order, each as what it was", async () => {
    const here = await place(ouiCode, files);
    // From left.txt: an interactive script on it, its preview, a comparison with its file and a calculator.
    await here.run(["left.txt"], [keys.tools, "i", keys.previousTab, keys.f9, keys.previousTab, keys.previousTab,
        keys.tools, "s", keys.tools, "l", "interes = 800*0.03\r", keys.quit]);
    const shown = await here.run([], ["interes + 1\r"]);
    await here.remove();
    const tabs = shown.row(1);
    const order = ["left.txt ×", "Interactive Script ×", "Preview: left.txt ×", "left.txt (saved) vs left.txt ×",
        "Calculator ×"].map((title) => tabs.indexOf(title));
    expect("the tabs, in order", JSON.stringify(order), JSON.stringify([...order].sort((a, b) => a - b)));
    expect("every one of them", order.includes(-1), false);
    // The calculator, shown as it was left, its values kept.
    expect("its log", shown.find("interes = 24") !== null, true);
    expect("its values", shown.find("= 25") !== null, true);
});

const descriptions = {
    "form.oui": "Window \"Ask Someone\" width=30\n  Label \"Their name:\"\n  LineEdit id=name\n  Slot id=list\n",
    "bad.oui": "Column\n  Slider\n",
};

test("Interactive UI builds the description open beside it, and says where one is wrong", async () => {
    const shown = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u"]);
    expect("the tab", shown.find("form.oui (interactive)") !== null, true);
    expect("the window it describes", shown.find("Ask Someone") !== null, true);
    expect("what is in it", shown.find("Their name:") !== null, true);
    expect("where a slot goes", shown.find("[list]") !== null, true);
    const wrong = await session(ouiCode, ["bad.oui"], descriptions, [keys.tools, "u"]);
    expect("the error", wrong.find("Line 2, column 3: There is no widget called Slider.") !== null, true);
});

test("Opening Interactive UI from the menu suggests nothing: the letter picked the menu's entry", async () => {
    const shown = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u", ""]);
    expect("the tab", shown.find("form.oui (interactive)") !== null, true);
    // The suggestions list widgets with "widget" beside each.
    expect("no suggestions", shown.find("widget"), null);
});

test("Interactive UI builds again what is typed, a moment after", async () => {
    // Ctrl+End, then a button at the end of the window.
    const shown = await session(ouiCode, ["form.oui"], descriptions,
        [keys.tools, "u", "\x1b[1;5F", "  Button \"Send It\"", "\r", ""]);
    expect("the new button", shown.find("Send It ") !== null && shown.find("Send It ").column > 50, true);
});

test("In a description a space after a widget suggests what it takes", async () => {
    // Ctrl+End, a new line, a label and a space.
    const shown = await session(ouiCode, ["form.oui"], descriptions,
        [keys.tools, "u", "\x1b[1;5F", "  Label \"hola mundo\"", " "]);
    expect("a property of a label", shown.find("buddy") !== null, true);
    expect("what it is", shown.find("the field its letter goes to") !== null, true);
});

test("Interactive UI comes back after quitting, and F1 there opens its part of the help", async () => {
    const here = await place(ouiCode, descriptions);
    await here.run(["form.oui"], [keys.tools, "u", keys.quit]);
    const back = await here.run([], []);
    const help = await here.run([], [keys.f1]);
    await here.remove();
    expect("the tab", back.find("form.oui (interactive)") !== null, true);
    expect("what it builds", back.find("Ask Someone") !== null, true);
    expect("the help", help.find("opens a tab with a user interface description") !== null, true);
});

test("Tools has the scripts OUI Code loads in a submenu of their own, User Scripts", async () => {
    const tools = await session(ouiCode, ["left.txt"], files, [keys.tools]);
    expect("the submenu", tools.find("User Scripts") !== null, true);
    expect("not loose in Tools", tools.find("Reload User Scripts"), null);
    const opened = await session(ouiCode, ["left.txt"], files, [keys.tools, "p"]);
    for (const title of ["Open User Script", "Reload User Scripts", "User Scripts Output"]) {
        expect(title, opened.find(title) !== null, true);
    }
});

// A user's script with text commands that say where in the menus they go.
const placed = {"oui_code_scripts/placed.js": `
oui.commands.register({id: "user.shout", title: "Shout", menuPath: {en: "edit/&Quirks", es: "edit/&Rarezas"},
                       run: (text) => text.toUpperCase()});
oui.commands.register({id: "user.top", title: "Top Level", menuPath: "EDIT", run: (text) => text});
oui.commands.register({id: "user.joined", title: "Joined", menuPath: "tools/Text Commands", run: (text) => text});
`};
// Tall enough for the whole of the Edit menu, where they go after its own entries.
const tall = {rows: 50};

test("A text command goes where its menuPath says: in a menu, in a submenu made for it, or in one of OUI Code's", async () => {
    const edit = await session(ouiCode, ["left.txt"], {...files, ...placed}, ["\x1be"], tall);
    expect("in Edit", edit.find("Top Level") !== null, true);
    expect("its submenu", edit.find("Quirks") !== null, true);
    expect("not loose", edit.find("Shout"), null);
    const quirks = await session(ouiCode, ["left.txt"], {...files, ...placed}, ["\x1be", "q"], tall);
    expect("in the submenu", quirks.find("Shout") !== null, true);
    const text = await session(ouiCode, ["left.txt"], {...files, ...placed}, [keys.tools, "x"], tall);
    expect("joined to Text Commands", text.find("Joined") !== null && text.find("Sort Lines") !== null, true);
    expect("only those with no place", text.find("Shout"), null);
    const run = await session(ouiCode, ["left.txt"], {...files, ...placed}, ["\x1be", "q", keys.enter], tall);
    expect("it runs from there", run.find("GONE") !== null, true);
});

test("In Spanish a menuPath names OUI Code's submenus in English or as they show, and its own as it says", async () => {
    const spanish = {...files, ...placed, "oui_code.ini": "language=es\n"};
    const edit = await session(ouiCode, ["left.txt"], spanish, ["\x1be"], tall);
    expect("its own, in Spanish", edit.find("Rarezas") !== null, true);
    const text = await session(ouiCode, ["left.txt"], spanish, ["\x1bh", "t"], tall);
    expect("found by its English name", text.find("Joined") !== null && text.find("Ordenar líneas") !== null, true);
});

test("The Edit menu has Find Next, Find Previous and Find Next of the Selection, each beside its key", async () => {
    const edit = await session(ouiCode, ["left.txt"], files, ["\x1be"], tall);
    const rows = Array.from({length: 50}, (_, index) => edit.row(index));
    for (const [title, key] of [["Find Next", "F3"], ["Find Previous", "Shift\\+F3"],
                                ["Find Next of the Selection", "Ctrl\\+F3"]]) {
        expect(title, rows.some((row) => new RegExp(`${title} +${key} `).test(row)), true);
    }
});

test("Alt+X lists the text commands alone, and the one picked works on the document", async () => {
    const listed = await session(ouiCode, ["left.txt"], files, ["\x1bx"]);
    expect("the list", listed.find("Text Commands") !== null, true);
    expect("a text command", listed.find("Sort Lines") !== null, true);
    expect("no other command", listed.find("Command Palette"), null);
    const shown = await session(ouiCode, ["left.txt"], files, ["\x1bx", "upper", keys.enter]);
    expect("the document, in upper case", shown.find("GONE") !== null, true);
});

// Tools of the user's, a script each in the tools folder of their scripts.
const tools = {
    "oui_code_scripts/tools/first_line.js": `
export const tool = {title: "First Line"};
export async function run({file, line, column}) {
    await null;
    console.log(\`first line: \${oui.readFile(file).split("\\n")[0]}, at \${line}:\${column}\`);
}
`,
    "oui_code_scripts/tools/shout.js": `
export const tool = {title: "Shout", menuPath: "edit/&Loud"};
export function run({selection}) { return selection.toUpperCase(); }
`,
};

test("A tool runs on the document saved, and what it says opens a tab of its own", async () => {
    // O is the letter of User Tools in the Tools menu; First Line, with no place of its own, is there.
    const shown = await session(ouiCode, ["left.txt"], {...files, ...tools}, ["X", keys.tools, "o", keys.enter]);
    expect("the file as the editor had it", shown.find("first line: Xone, at 1:2") !== null, true);
    expect("its tab", shown.find("First Line on left.txt") !== null, true);
});

test("Text a tool gives back replaces the selection, and a tool that says nothing opens no tab", async () => {
    // Shift+End selects the first line; Edit > Loud > Shout, where its menuPath puts it.
    const shown = await session(ouiCode, ["left.txt"], {...files, ...tools},
        ["\x1b[1;2F", "\x1be", "l", keys.enter], tall);
    expect("in upper case", shown.find("ONE") !== null, true);
    expect("the next line as it was", shown.find("gone") !== null, true);
    expect("no tab", shown.find("Shout on"), null);
    const menu = await session(ouiCode, ["left.txt"], {...files, ...tools}, [keys.tools, "o"]);
    expect("in User Tools, the one with no place", menu.find("First Line") !== null, true);
    expect("not the one with a place", menu.find("Shout"), null);
});

test("A tool still running that has said nothing is kept, and shows itself when another would start", async () => {
    // Short enough to be gone when the session ends.
    const slow = {...files, "oui_code_scripts/tools/slow.js":
        `export const tool = {title: "Slow"};\nexport async function run() { await oui.run("sleep 3"); }\n`};
    const shown = await session(ouiCode, ["left.txt"], slow, [keys.tools, "o", keys.enter, keys.tools, "o", keys.enter]);
    expect("why not", shown.find("A script is running already") !== null, true);
    expect("its tab, to stop it", shown.find("Slow on left.txt") !== null, true);
});

test("A script in the tools folder that is no tool is said, and the rest load", async () => {
    const broken = {...files, ...tools, "oui_code_scripts/tools/broken.js": "export function process(dryRun) {}\n"};
    const shown = await session(ouiCode, ["left.txt"], broken, []);
    expect("what is wrong", shown.find("tools/broken.js is in the tools folder, but is no tool") !== null, true);
});

test("Ctrl+T offers every kind of tab: the calculator among them", async () => {
    const shown = await session(ouiCode, ["left.txt"], files, ["\x14"]);
    for (const title of ["New File", "New Terminal", "Calculator", "Interactive Script", "Interactive UI",
        "Compare Files...", "Task List", "Code TODOs"]) {
        expect(title, shown.find(title) !== null, true);
    }
    const opened = await session(ouiCode, ["left.txt"], files, ["\x14", "calc", keys.enter]);
    expect("the calculator's tab", opened.find("Calculator ×") !== null, true);
});

test("Alt+K and an arrow go from a description to the interface it builds, to type in it", async () => {
    // The description's field, on the right: Alt+K, right, and what is typed goes in it.
    const shown = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u", "\x1bk", "\x1b[C", "typed"]);
    expect("in the field on the right", shown.find("typed") !== null && shown.find("typed").column > 50, true);
});

test("Alt and the key under Escape open a terminal too, as a terminal cannot send Ctrl with that key", async () => {
    // Escape before the key: Alt, as a terminal sends it. º on a Spanish keyboard, ` on an English one.
    for (const key of ["º", "`"]) {
        const shown = await session(ouiCode, ["left.txt"], files, [`\x1b${key}`, ""]);
        expect(`the terminal's tab, Alt+${key}`, shown.row(1).split("×").length - 1, 2);
        expect(`in front, Alt+${key}`, shown.find("gone"), null);
    }
});

test("Alt+K and Page Up leave a terminal tab for the tab before", async () => {
    // A terminal from Ctrl+T, then Alt+K and Page Up.
    const shown = await session(ouiCode, ["left.txt"], files,
        ["\x14", "terminal", keys.enter, "", "\x1bk", "\x1b[5~", keys.enter]);
    // Back on left.txt, the terminal's tab still there beside it.
    expect("two tabs", shown.row(1).split("×").length - 1, 2);
    expect("the document", shown.find("gone") !== null, true);
});

test("F4 into an Interactive UI tab writes in its description, with no suggestions out of place", async () => {
    // F4 goes round the groups; with one, back to the same tab: then what is typed goes in the description.
    const shown = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u", "\x1bOS", "QQ"]);
    const typed = shown.find("QQ");
    expect("written in the description", typed !== null && typed.column < 45, true);
    // The list would show every widget there is, Column among them, which the description does not have.
    expect("no list of widgets", shown.find("Column"), null);
});

test("F4 into the calculator's tab writes at its prompt", async () => {
    const shown = await session(ouiCode, ["left.txt"], files, [keys.tools, "l", "\x1bOS", "6*7\r"]);
    expect("its answer", shown.find("42") !== null, true);
});

test("Interactive UI asked for again goes to the tab there is, rather than make another", async () => {
    // From the description, from its interactive tab, and from the description again.
    const shown = await session(ouiCode, ["form.oui"], descriptions,
        [keys.tools, "u", keys.tools, "u", keys.previousTab, keys.tools, "u"]);
    expect("the tabs", shown.row(1).trim(), "form.oui ×  form.oui (interactive) ×");
});

test("The status line offers Save only where there is a document to save", async () => {
    const documentShown = await session(ouiCode, ["left.txt"], files, [""], {columns: 120, rows: 24});
    expect("on a document", documentShown.row(23).includes("Save"), true);
    const calculator = await session(ouiCode, ["left.txt"], files, [keys.tools, "l"], {columns: 120, rows: 24});
    expect("on the calculator", calculator.row(23).includes("Save"), false);
});

test("Typing in a dialog over a description suggests nothing in the description", async () => {
    // Ctrl+T over a .oui: what is typed filters the list, and Enter picks from it.
    const shown = await session(ouiCode, ["form.oui"], descriptions, ["\x14", "interactive ui", keys.enter]);
    expect("the tab picked", shown.find("form.oui (interactive)") !== null, true);
    expect("the description as it was", shown.find("Window \"Ask Someone\" width=30") !== null, true);
});

test("Generate in an Interactive UI tab writes a script for oui_run in a new tab", async () => {
    // Where the button is, then a click on it.
    const before = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u"]);
    const button = before.find("Generate");
    expect("the button", button !== null, true);
    const click = (press) => `\x1b[<0;${button.column + 2};${button.row + 1}${press ? "M" : "m"}`;
    const shown = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u", click(true), click(false)]);
    expect("a new tab", shown.row(1).includes("Untitled"), true);
    expect("loading the description", shown.find("oui.uiFromFile(\"form.oui\")") !== null, true);
    expect("its window opened", shown.find("oui.window(ui);") !== null, true);
});

test("Generate writes in the language chosen last, remembered: C++ here", async () => {
    const remembered = {...descriptions, "oui_code.ini": "ui_generate_language=cpp\n"};
    const before = await session(ouiCode, ["form.oui"], remembered, [keys.tools, "u"]);
    expect("the language shown", before.find("C++") !== null, true);
    const button = before.find("Generate");
    const click = (press) => `\x1b[<0;${button.column + 2};${button.row + 1}${press ? "M" : "m"}`;
    const shown = await session(ouiCode, ["form.oui"], remembered, [keys.tools, "u", click(true), click(false)]);
    expect("C++ in a new tab", shown.find("void show_form(oui::Application& application") !== null, true);
});

test("Generate runs the script chosen for its language on what it wrote, and shows what the script says", async () => {
    const withScript = {...descriptions,
        "oui_code.ini": "ui_generate_language=cpp\nui_generate_script_cpp=after.js\n",
        "after.js": "export function processGeneratedCode(code, {language, file}) {\n" +
            "    console.log('got', language, file.endsWith('form.oui'), code.includes('show_form'));\n" +
            "    return 'Wrote it all.';\n" +
            "}\n"};
    const before = await session(ouiCode, ["form.oui"], withScript, [keys.tools, "u"]);
    expect("the script shown", before.find("after.js") !== null, true);
    const button = before.find("Generate");
    const click = (press) => `\x1b[<0;${button.column + 2};${button.row + 1}${press ? "M" : "m"}`;
    const shown = await session(ouiCode, ["form.oui"], withScript, [keys.tools, "u", click(true), click(false)]);
    expect("the code's tab", shown.row(1).includes("Untitled"), true);
    expect("the output's tab", shown.find("after.js on form.oui") !== null, true);
    expect("what it printed", shown.find("got cpp true true") !== null, true);
    expect("what it gave back", shown.find("Wrote it all.") !== null, true);
});

test("An interactive script runs on text pasted in its tab, and Apply puts its result there", async () => {
    const scripts = {"upper.js": "export function transform(text) {\n    return text.toUpperCase();\n}\n"};
    // From the script: what it runs on, picked by what it says.
    const open = [keys.tools, "i", "pasted", keys.enter];
    const before = await session(ouiCode, ["upper.js"], scripts, open);
    const input = before.find("Input");
    expect("the input's pane", input !== null, true);
    const click = [`\x1b[<0;${input.column + 2};${input.row + 2}M`, `\x1b[<0;${input.column + 2};${input.row + 2}m`];
    const shown = await session(ouiCode, ["upper.js"], scripts, [...open, ...click, "hola ñ", ""]);
    expect("the result", shown.find("HOLA Ñ") !== null, true);
    expect("its title", shown.find("Result on Input") !== null, true);
    const apply = shown.find("Apply");
    expect("Apply", apply !== null, true);
    const applied = await session(ouiCode, ["upper.js"], scripts, [...open, ...click, "hola ñ", "",
        `\x1b[<0;${apply.column + 2};${apply.row + 1}M`, `\x1b[<0;${apply.column + 2};${apply.row + 1}m`]);
    expect("the input, applied", applied.row(input.row + 1).includes("HOLA Ñ"), true);
});

test("Text pasted in an interactive script's tab comes back after quitting", async () => {
    const scripts = {"upper.js": "export function transform(text) {\n    return text.toUpperCase();\n}\n"};
    const open = [keys.tools, "i", "pasted", keys.enter];
    const before = await session(ouiCode, ["upper.js"], scripts, open);
    const input = before.find("Input");
    const click = [`\x1b[<0;${input.column + 2};${input.row + 2}M`, `\x1b[<0;${input.column + 2};${input.row + 2}m`];
    const here = await place(ouiCode, scripts);
    await here.run(["upper.js"], [...open, ...click, "kept text", "", keys.quit]);
    const shown = await here.run([], [""]);
    await here.remove();
    expect("the text", shown.find("kept text") !== null, true);
    expect("the result on it", shown.find("KEPT TEXT") !== null, true);
});

test("New beside Generate starts a script on what Generate writes, and saved it is the one that runs after", async () => {
    const before = await session(ouiCode, ["form.oui"], descriptions, [keys.tools, "u"]);
    const box = before.find("No Script");
    const click = (row) => [`\x1b[<0;${box.column + 2};${row + 1}M`, `\x1b[<0;${box.column + 2};${row + 1}m`];
    // Open the box, then its third option: No Script, Choose..., New...
    const create = [keys.tools, "u", ...click(box.row), ...click(box.row + 2), ""];
    const shown = await session(ouiCode, ["form.oui"], descriptions, create);
    expect("the tab", shown.row(1).includes("Interactive Script"), true);
    expect("the template", shown.find("export async function processGeneratedCode") !== null, true);
    expect("what Generate writes, as its input", shown.find("oui.uiFromFile(\"form.oui\")") !== null, true);
    expect("what it would do", shown.find("Would write") !== null, true);
    const asking = await session(ouiCode, ["form.oui"], descriptions, [...create, "\x13"]);
    expect("the name offered", asking.find("untitled.js") !== null, true);
    const saved = await session(ouiCode, ["form.oui"], descriptions,
        [...create, "\x13", keys.enter, keys.previousTab]);
    // On the row of the box, not the tab's title, which says the name too.
    expect("the script beside Generate", saved.row(box.row).includes("untitled.js"), true);
});

test("Generate on a description not saved yet saves it first, as a .oui, and loads it by that name", async () => {
    const before = await session(ouiCode, ["left.txt"], files, [keys.tools, "u"]);
    const button = before.find("Generate");
    const click = (press) => `\x1b[<0;${button.column + 2};${button.row + 1}${press ? "M" : "m"}`;
    const asking = await session(ouiCode, ["left.txt"], files, [keys.tools, "u", click(true), click(false)]);
    expect("the name offered", asking.find("untitled.oui") !== null, true);
    const shown = await session(ouiCode, ["left.txt"], files, [keys.tools, "u", click(true), click(false), keys.enter]);
    expect("the script loading it", shown.find("oui.uiFromFile(\"untitled.oui\")") !== null, true);
});

await run(cases, only);
