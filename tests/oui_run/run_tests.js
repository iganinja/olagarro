// oui_run's own tests, run by oui_run: oui_run tests/oui_run/run_tests.js PATH_TO_OUI_RUN
// Each case is a script run by the oui_run given, in a folder of its own, and what it prints and the code it ends
// with are compared with what they should be.

import {capture, expect, place, quote, run} from "../lib/terminal_session.js";

const [ouiRun] = oui.args;
if (!ouiRun) {
    console.log("usage: oui_run tests/oui_run/run_tests.js PATH_TO_OUI_RUN");
    oui.exit(2);
}

// A folder of its own for each case, removed after.
async function scratch() {
    const {output} = await capture("mktemp -d");
    return output.trim();
}

const cases = [];
const test = (name, run) => cases.push({name, run});

// Runs script, written as a file in a folder of its own with the files given beside it, and gives what it printed
// and its exit code.
async function runScript(script, files = {}) {
    const folder = await scratch();
    for (const [name, text] of Object.entries({"main.js": script, ...files})) {
        oui.writeFile(`${folder}/${name}`, text);
    }
    const result = await capture(`${quote(ouiRun)} main.js </dev/null`, {cwd: folder});
    await capture(`rm -rf ${quote(folder)}`);
    return result;
}

// As runScript, with input given to what the script reads.
async function runScriptWith(script, input) {
    const folder = await scratch();
    oui.writeFile(`${folder}/main.js`, script);
    const result = await capture(`${quote(ouiRun)} main.js`, {cwd: folder, input});
    await capture(`rm -rf ${quote(folder)}`);
    return result;
}

test("exit ends the script there", async () => {
    const {code, output} = await runScript(`oui.exit(3);\nconsole.log("after");\n`);
    expect("code", code, 3);
    expect("output", output, "");
});

test("exit is not caught by a catch", async () => {
    const {code, output} = await runScript(
        `try { oui.exit(4); } catch { console.log("caught"); } finally { console.log("finally"); }\n` +
        `console.log("after");\n`);
    expect("code", code, 4);
    expect("output", output, "");
});

test("exit after an await ends the script there", async () => {
    const {code, output} = await runScript(`await oui.run("true");\noui.exit(5);\nconsole.log("after");\n`);
    expect("code", code, 5);
    expect("output", output, "");
});

test("exit in a timer ends everything to come", async () => {
    const {code, output} = await runScript(
        `setTimeout(() => { oui.exit(6); console.log("after"); }, 1);\n` +
        `setTimeout(() => console.log("later"), 200);\n`);
    expect("code", code, 6);
    expect("output", output, "");
});

test("exit in an async function ends the script there", async () => {
    const {code, output} = await runScript(
        `async function finish() { await null; oui.exit(7); console.log("after"); }\n` +
        `finish().catch(() => console.log("rejected"));\n`);
    expect("code", code, 7);
    expect("output", output, "");
});

test("exit without a code ends with 0", async () => {
    const {code, output} = await runScript(`oui.exit();\nthrow new Error("not reached");\n`);
    expect("code", code, 0);
    expect("output", output, "");
});

// What a script's window puts on a terminal 40 columns by 10 rows, escape sequences and all, run in a pseudo terminal
// by script(1), the script ending it a moment after it opens.
async function inTerminal(script) {
    const folder = await scratch();
    oui.writeFile(`${folder}/main.js`, `${script}\nsetTimeout(() => oui.quit(), 300);\n`);
    const inner = `stty cols 40 rows 10; ${quote(ouiRun)} main.js`;
    const {output} = await capture(`script -qec ${quote(inner)} /dev/null`, {cwd: folder});
    await capture(`rm -rf ${quote(folder)}`);
    return output;
}

// The colours text is drawn in, as the SGR sequence before it: "0;97;44" for white on blue.
const sgrBefore = (screen, text) => new RegExp(`\\x1b\\[([\\d;]*)m(?:\\x1b\\[\\d+;\\d+H)?${text}`).exec(screen)?.[1];

test("a window in the terminal is in Turbo's colours: a field white on blue", async () => {
    const screen = await inTerminal(`const go = new oui.Button("Go");\n` +
        `oui.window("T", oui.column([new oui.LineEdit({text: "fieldtext"}), go]));\ngo.focus();`);
    expect("field", sgrBefore(screen, "fieldtext"), "0;97;44");
});

test("a list is its colour from one side to the other, not only under its text", async () => {
    const screen = await inTerminal(`const go = new oui.Button("Go");\n` +
        `oui.window("T", oui.column([oui.fill(new oui.List(["first", "secondtext"])), go]));\ngo.focus();`);
    expect("text", sgrBefore(screen, "secondtext"), "0;30;46");
    // What follows the text on its row: spaces in the same background.
    const rest = /secondtext(?:\x1b\[\d+;\d+H)?\x1b\[0;\d+;(\d+)m +/.exec(screen)?.[1];
    expect("after the text", rest, "46");
});

test("oui.ui builds what a description says, its widgets by their ids, its texts as written", async () => {
    const {code, output} = await runScript(
        "const ui = oui.ui(`Column\n" +
        "  Label \"Hello\" id=greeting\n" +
        "  LineEdit id=name text=\"Ana\"\n" +
        "  CheckButton \"Loud\" id=loud checked\n" +
        "  ComboBox id=size\n" +
        "  Slot id=choices\n" +
        "`);\n" +
        "ui.size.items = [\"small\", \"big\"];\n" +
        "ui.size.selected = 1;\n" +
        "ui.loud.checked = !ui.loud.checked;\n" +
        "ui.put(\"choices\", new oui.List([\"a\", \"b\"]));\n" +
        "ui.choices.selected = 1;\n" +
        "console.log(ui.greeting.text, ui.name.text, ui.loud.checked, ui.size.selected, ui.choices.selected,\n" +
        "    ui.root instanceof oui.Widget, ui.loud instanceof oui.CheckButton);\n");
    expect("code", code, 0);
    expect("output", output, "Hello Ana false 1 1 true true\n");
});

test("tabs, splits and Markdown from a description answer to a script", async () => {
    const {code, output} = await runScript(
        "const ui = oui.ui(`Split id=sides ratio=0.25\n" +
        "  Tabs id=pages fixed\n" +
        "    Tab \"One\"\n" +
        "      Label \"first\"\n" +
        "    Tab \"Two\"\n" +
        "      Label \"second\"\n" +
        "  MarkdownView id=page\n" +
        "`);\n" +
        "const chosen = [];\n" +
        "ui.pages.onSelect = (index) => chosen.push(index);\n" +
        "ui.pages.selected = 1;\n" +
        "ui.sides.ratio = 0.5;\n" +
        "ui.page.markdown = \"# Title\";\n" +
        "console.log(ui.pages.count, ui.pages.selected, chosen.join(), ui.sides.ratio, ui.page.markdown,\n" +
        "    ui.pages instanceof oui.Tabs, ui.sides instanceof oui.Split, ui.page instanceof oui.MarkdownView);\n");
    expect("code", code, 0);
    expect("output", output, "2 1 1 0.5 # Title true true true\n");
});

test("tabs, splits and Markdown made in code", async () => {
    const {code, output} = await runScript(
        "const tabs = new oui.Tabs({fixed: true});\n" +
        "tabs.add(\"A\", new oui.Label(\"a\"));\n" +
        "tabs.add(\"B\", new oui.MarkdownView(\"*b*\"));\n" +
        "const sides = oui.split(tabs, new oui.Label(\"c\"), {ratio: 0.3});\n" +
        "console.log(tabs.count, tabs.selected, sides.ratio, sides instanceof oui.Split);\n");
    expect("code", code, 0);
    expect("output", output, "2 0 0.3 true\n");
});

test("a page a script chooses is the one on show", async () => {
    const screen = await inTerminal("const tabs = new oui.Tabs({fixed: true})\n" +
        "    .add(\"One\", new oui.Label(\"firstpage\")).add(\"Two\", new oui.Label(\"secondpage\"));\n" +
        "oui.window(\"T\", tabs);\ntabs.selected = 1;");
    expect("the second page", screen.includes("secondpage"), true);
});

test("oui.uiFromFile reads a description beside the script, from wherever it is run", async () => {
    const folder = await scratch();
    oui.writeFile(`${folder}/main.js`, `console.log(oui.uiFromFile("ui/form.oui").title.text);\n`);
    await capture("mkdir ui elsewhere", {cwd: folder});
    oui.writeFile(`${folder}/ui/form.oui`, `# A form.\nColumn\n  Label "From the file" id=title\n`);
    const {code, output} = await capture(`${quote(ouiRun)} ../main.js </dev/null`, {cwd: `${folder}/elsewhere`});
    await capture(`rm -rf ${quote(folder)}`);
    expect("code", code, 0);
    expect("output", output, "From the file\n");
});

test("a description that is not one says where it is wrong", async () => {
    const {code, output} = await runScript(
        "try { oui.ui(\"Column\\n  Slider\\n\"); } catch (error) { console.log(error.message); }\n" +
        "try { oui.uiFromFile(\"bad.oui\"); } catch (error) { console.log(error.message); }\n" +
        "try { oui.uiFromFile(\"none.oui\"); } catch (error) { console.log(error.message); }\n",
        {"bad.oui": "Column\n  Label colour=red\n"});
    expect("code", code, 0);
    expect("output", output, "oui.ui: 2:3: There is no widget called Slider.\n" +
        "bad.oui:2:9: A Label has no property colour.\n" +
        "oui.uiFromFile: cannot read none.oui\n");
});

test("a described window in the terminal is as one built in code: a field white on blue", async () => {
    const screen = await inTerminal("const ui = oui.ui(`Window \"T\"\n" +
        "  LineEdit id=field text=\"fieldtext\"\n  Button \"Go\" id=go\n`);\noui.window(ui);\nui.go.focus();");
    expect("field", sgrBefore(screen, "fieldtext"), "0;97;44");
});

test("a window whose terminal goes away ends the program, rather than spin", async () => {
    const folder = await scratch();
    oui.writeFile(`${folder}/main.js`, `oui.window("T", new oui.Label("waiting"));\n`);
    // The terminal is killed under it, as a dropped SSH connection or a closed terminal window leaves it.
    const inner = `stty cols 40 rows 10; ${quote(ouiRun)} ${quote(`${folder}/main.js`)}`;
    await capture(`(sleep 1.5 | timeout -s KILL 1 script -qec ${quote(inner)} /dev/null) >/dev/null 2>&1`);
    await capture("sleep 1");
    // What is still running the script, found apart from the command that looks, as pgrep -f would find itself.
    const {output} = await capture("ps -eo pid=,args=");
    const left = output.split("\n").filter((line) => line.includes(`${folder}/main.js`));
    for (const line of left) {
        await capture(`kill -9 ${line.trim().split(" ")[0]}`);
    }
    await capture(`rm -rf ${quote(folder)}`);
    expect("still running", left.length, 0);
});

// The recipes in examples/oui_run/recipes, each run on a folder with these files in it, as a user would.
const root = (await capture("pwd")).output.trim();
const recipes = `${root}/examples/oui_run/recipes`;
const fixture = {
    "notes/intro.md": "# Intro\n\nSee [the guide](guide.md) and [setup](setup.md#start).\n\n## Getting started\n\n" +
        "TODO: write this part\n\n## Getting started\n\n```sh\n# not a heading\n```\n\n### Details\n",
    "notes/guide.md": "# Guide\n\nBack to [intro](intro.md#intro), [home](https://example.com), ![logo](logo.png).\n",
    "src/main.js": "// TODO: read the settings\nconst colour = \"red\";   \nconsole.log(colour);\t\n" +
        "/* FIXME: colour by name */\n\n\n",
    "src/util.js": "// HACK: the colour of the sky\nexport const sky = \"blue\";\n",
    "data/sales.csv": "region,product,amount\nnorth,apples,10.5\nsouth,\"pears, green\",4\nnorth,pears,2\n" +
        "east,apples,x\nsouth,apples,7.25\n",
    "data/people.json": JSON.stringify([
        {name: "Lucía", team: "tools", age: 31},
        {name: "Ana", team: "engine", age: 28, langs: ["C++", "Lua"]},
        {name: "Bruno", team: "tools", note: "says \"hi\", often"},
    ], null, 2) + "\n",
    "data/blob.bin": "abc\0TODO, but not in text\n",
    "photos/IMG_001.JPG": "",
    "photos/IMG_002.JPG": "",
    "photos/readme.txt": "Photos.\n",
};

// Runs a recipe with args, each given to the shell quoted, in a folder with the fixture in it, and gives what it
// printed, its exit code, and the folder's files after, by path.
async function recipe(script, args = [], {input = "", files = {}, driver} = {}) {
    const folder = await scratch();
    for (const [path, text] of Object.entries({...fixture, ...files})) {
        await capture(`mkdir -p ${quote(path.slice(0, path.lastIndexOf("/")))}`, {cwd: folder});
        oui.writeFile(`${folder}/${path}`, text);
    }
    if (driver) {
        oui.writeFile(`${folder}/driver.js`, driver);
    }
    const command = [ouiRun, driver ? "driver.js" : `${recipes}/${script}`, ...args].map(quote).join(" ");
    const result = await capture(command, {cwd: folder, input});
    const after = {};
    for (const path of (await capture("find . -type f | sort", {cwd: folder})).output.split("\n").filter(Boolean)) {
        after[path.slice(2)] = oui.readFile(`${folder}/${path}`);
    }
    await capture(`rm -rf ${quote(folder)}`);
    return {...result, after};
}

// What an OUI Code script does with what Run Script gives it, run by a script that imports it and calls it.
const calling = (script, call) =>
    `import * as script from ${JSON.stringify(`${recipes}/oui_code/${script}`)};\n${call}\n`;

// As wc -l -w -m counts them.
test("count counts lines, words and characters, a total under them", async () => {
    const {code, output} = await recipe("count.js", ["src", "notes", "--include=*.js,*.md"]);
    expect("code", code, 0);
    expect("output", output,
        "lines  words  chars  file\n" +
        "    6     16    104  src/main.js\n" +
        "    2     12     58  src/util.js\n" +
        "    3      7     90  notes/guide.md\n" +
        "   15     25    167  notes/intro.md\n" +
        "   26     60    419  total\n");
});

test("search lists the lines it finds, and says where", async () => {
    const {code, output} = await recipe("search.js", ["colou?r", "src"]);
    expect("code", code, 0);
    expect("output", output,
        "src/main.js:2: const colour = \"red\";\n" +
        "src/main.js:3: console.log(colour);\n" +
        "src/main.js:4: /* FIXME: colour by name */\n" +
        "src/util.js:1: // HACK: the colour of the sky\n" +
        "4 matches in 2 files.\n");
});

test("search leaves binary files out, and ends with 1 when it finds nothing", async () => {
    const {code, output} = await recipe("search.js", ["TODO", "data"]);
    expect("code", code, 1);
    expect("output", output, "No matches.\n");
});

test("replace without --write shows what it would change, and changes nothing", async () => {
    const {code, output, after} = await recipe("replace.js", ["colou?r", "hue", "src"]);
    expect("code", code, 0);
    expect("output", output,
        "src/main.js:2\n  - const colour = \"red\";\n  + const hue = \"red\";\n" +
        "src/main.js:3\n  - console.log(colour);\n  + console.log(hue);\n" +
        "src/main.js:4\n  - /* FIXME: colour by name */\n  + /* FIXME: hue by name */\n" +
        "src/util.js:1\n  - // HACK: the colour of the sky\n  + // HACK: the hue of the sky\n" +
        "Would replace 4 in 2 files. Again with --write to do it.\n");
    expect("src/main.js", after["src/main.js"], fixture["src/main.js"]);
});

test("replace with --write asks, then writes what it captured where it is told", async () => {
    const {code, after} = await recipe("replace.js", ["(\\w+)our", "$1or", "src", "--write"], {input: "y\n"});
    expect("code", code, 0);
    expect("src/util.js", after["src/util.js"], "// HACK: the color of the sky\nexport const sky = \"blue\";\n");
});

test("replace with --write changes nothing when told no", async () => {
    const {code, output, after} = await recipe("replace.js", ["colour", "hue", "src", "--write"], {input: "n\n"});
    expect("code", code, 0);
    // The question's line ends with the answer, which the terminal shows and a pipe does not.
    expect("end", output.endsWith("[y/N] Nothing changed.\n"), true);
    expect("src/util.js", after["src/util.js"], fixture["src/util.js"]);
});

test("csv_summary adds up a column by another, quoted fields and all", async () => {
    const {code, output} = await recipe("csv_summary.js", ["data/sales.csv", "region", "amount"]);
    expect("code", code, 0);
    expect("output", output,
        "Not a number, left out: east,apples,x\n" +
        "region  amount\n" +
        "north     12.5\n" +
        "south    11.25\n");
});

test("csv_summary counts rows without a column to add up", async () => {
    const {code, output} = await recipe("csv_summary.js", ["data/sales.csv", "product"]);
    expect("code", code, 0);
    expect("output", output, "product       rows\napples           3\npears            1\npears, green     1\n");
});

test("csv_summary names the columns there are when given one that is not", async () => {
    const {code, output} = await recipe("csv_summary.js", ["data/sales.csv", "city"]);
    expect("code", code, 1);
    expect("output", output, "data/sales.csv has no column \"city\"; it has region, product, amount\n");
});

test("json_to_csv makes a column of every field, and quotes what needs it", async () => {
    const {code, output} = await recipe("json_to_csv.js", ["data/people.json"]);
    expect("code", code, 0);
    expect("output", output,
        "name,team,age,langs,note\n" +
        "Lucía,tools,31,,\n" +
        "Ana,engine,28,\"[\"\"C++\"\",\"\"Lua\"\"]\",\n" +
        "Bruno,tools,,,\"says \"\"hi\"\", often\"\n");
});

test("json_to_csv filters and sorts, and writes the file", async () => {
    const {code, output, after} = await recipe("json_to_csv.js",
        ["data/people.json", "out.csv", "--where=team=tools", "--sort=name"]);
    expect("code", code, 0);
    expect("output", output, "out.csv: 2 rows, 4 columns\n");
    expect("out.csv", after["out.csv"],
        "name,team,note,age\nBruno,tools,\"says \"\"hi\"\", often\",\nLucía,tools,,31\n");
});

test("tally counts the lines a command prints, most first", async () => {
    const {code, output} = await recipe("tally.js", ["printf 'b\\na\\nb\\n'"]);
    expect("code", code, 0);
    expect("output", output, "count     %  line\n    2  66.7  b\n    1  33.3  a\n");
});

test("tally shows what a failed command said, and ends with 1", async () => {
    const {code, output} = await recipe("tally.js", ["echo nope; exit 3"]);
    expect("code", code, 1);
    expect("output", output, "\"echo nope; exit 3\" ended with 3:\nnope\n");
});

test("todo_report groups the tags in Markdown, binary files left out", async () => {
    const {code, output} = await recipe("todo_report.js", ["."]);
    expect("code", code, 0);
    expect("output", output,
        "# Things to do in .\n\n4 in all.\n\n" +
        "## FIXME (1)\n\n- [src/main.js:4](src/main.js#L4) colour by name\n\n" +
        "## TODO (2)\n\n- [notes/intro.md:7](notes/intro.md#L7) write this part\n" +
        "- [src/main.js:1](src/main.js#L1) read the settings\n\n" +
        "## HACK (1)\n\n- [src/util.js:1](src/util.js#L1) the colour of the sky\n");
});

test("rename without --write lists what it would rename", async () => {
    const {code, output, after} = await recipe("rename.js", ["^IMG_(\\d+)\\.JPG$", "photo_$1.jpg", "photos"]);
    expect("code", code, 0);
    expect("output", output,
        "IMG_001.JPG -> photo_001.jpg\nIMG_002.JPG -> photo_002.jpg\n" +
        "Would rename 2 files. Again with --write to do it.\n");
    expect("photos/IMG_001.JPG", after["photos/IMG_001.JPG"], "");
});

test("rename with --write renames, names with quotes in them too", async () => {
    const {code, after} = await recipe("rename.js", ["^IMG_", "it's ", "photos", "--write"]);
    expect("code", code, 0);
    expect("files", Object.keys(after).filter((path) => path.startsWith("photos/")).sort().join(" | "),
        "photos/it's 001.JPG | photos/it's 002.JPG | photos/readme.txt");
});

test("rename stops before anything when a new name is taken", async () => {
    const {code, output, after} = await recipe("rename.js", ["^IMG_(\\d+)\\.JPG$", "readme.txt", "photos", "--write"]);
    expect("code", code, 1);
    expect("output", output,
        "IMG_001.JPG -> readme.txt: that name is taken\nIMG_002.JPG -> readme.txt: that name is taken\n");
    expect("photos/IMG_001.JPG", after["photos/IMG_001.JPG"], "");
});

test("rename_window shows what the names come to as they are typed, and Rename renames them", async () => {
    const here = await place(ouiRun, {"photos/IMG_001.JPG": "", "photos/IMG_002.JPG": "", "photos/IMG_002.txt": ""});
    const shown = await here.run([`${recipes}/rename_window.js`, "photos"],
        ["^IMG_(\\d+)\\.JPG$", "\t", "photo_$1.jpg"], {columns: 80, rows: 20});
    const before = oui.listDir(`${here.folder}/photos`).sort().join(" ");
    await here.run([`${recipes}/rename_window.js`, "photos"],
        ["^IMG_(\\d+)\\.JPG$", "\t", "photo_$1.jpg", "\x1br"], {columns: 80, rows: 20});
    const after = oui.listDir(`${here.folder}/photos`).sort().join(" ");
    await here.remove();
    expect("preview", shown.find("IMG_001.JPG -> photo_001.jpg") !== null, true);
    expect("status", shown.find("2 files to rename.") !== null, true);
    expect("before Rename", before, "IMG_001.JPG IMG_002.JPG IMG_002.txt");
    expect("after Rename", after, "IMG_002.txt photo_001.jpg photo_002.jpg");
});

// A script run as main.js in a terminal 60 by 14, keys pressed as a person would, and the screen it leaves.
async function pressing(script, keys, files = {}) {
    const here = await place(ouiRun, {"main.js": script, ...files});
    const shown = await here.run(["main.js"], keys, {columns: 60, rows: 14});
    await here.remove();
    return shown;
}

test("a widget hides, a disabled one answers nothing, and a button's text changes", async () => {
    const shown = await pressing(`
const hidden = new oui.Label("not shown");
const go = new oui.Button("&Go", () => oui.status("went"));
const rename = new oui.Button("Rename", () => { rename.text = "Renamed"; });
oui.window("T", oui.column([hidden, go, rename]));
hidden.visible = false;
go.enabled = false;
oui.status(go.enabled ? "enabled" : "disabled", hidden.visible ? "shown" : "hidden");
rename.focus();
`, ["\x1bg", "\r", ""]);
    expect("hidden", shown.find("not shown"), null);
    expect("told", shown.find("disabled") !== null && shown.find("hidden") !== null, true);
    expect("Alt+G on the disabled one", shown.find("went"), null);
    expect("the new text", shown.find("Renamed") !== null, true);
});

test("a password field shows dots for what is typed, and the script gets the text", async () => {
    const shown = await pressing(`
const secret = new oui.LineEdit({password: true, onChange: (text) => oui.status(text === "s3cret" ? "right" : text.length + " typed")});
const plain = new oui.LineEdit();
oui.window("T", oui.column([secret, plain]));
plain.password = true;
oui.status(secret.password && plain.password ? "both" : "not both");
secret.focus();
`, ["s3cret", ""]);
    expect("the text", shown.find("s3cret"), null);
    expect("the dots", shown.find("●●●●●●") !== null, true);
    expect("what the script got", shown.find("right") !== null, true);
});

test("a prompt for a password with no window does not show what is typed", async () => {
    const shown = await pressing(`
const answer = await oui.prompt("Key", "Passphrase:", "", {password: true});
console.log(answer === "s3cret" ? "got it" : "got " + answer);
`, ["s3cret\r", ""]);
    expect("the text", shown.find("s3cret"), null);
    expect("what the script got", shown.find("got it") !== null, true);
});

test("a menu bar runs its entries, from the menu and by their keys, and the status line says what they do", async () => {
    const script = `
oui.menuBar([{title: "&File", items: [{title: "&Say", keys: "Ctrl+T", run: () => oui.status("said")}, "-",
                                      {title: "&Quit", run: () => oui.quit()}]}]);
oui.window("T", new oui.Label("body"));
oui.status("ready");
`;
    const start = await pressing(script, [""]);
    const menu = await pressing(script, ["\x1bf", "s", ""]);
    const keys = await pressing(script, ["\x14", ""]);
    expect("the bar", start.row(0).includes("File"), true);
    expect("the status line", start.row(13).includes("ready"), true);
    expect("from the menu", menu.row(13).includes("said"), true);
    expect("by its keys", keys.row(13).includes("said"), true);
});

test("a tree opens, says where the node chosen is, and a list's boxes tick", async () => {
    const tree = await pressing(`
const tree = new oui.Tree([{text: "fruit", expanded: true, children: [{text: "apples"}, {text: "pears"}]},
                           {text: "vegetables"}],
                          {onSelect: (path) => oui.status(JSON.stringify(path))});
oui.window("T", tree);
tree.focus();
`, ["\x1b[B", "\x1b[B", ""]);
    expect("the nodes", tree.find("apples") !== null && tree.find("vegetables") !== null, true);
    expect("where", tree.row(13).includes("[0,1]"), true);
    const list = await pressing(`
const list = new oui.List(["bread", "milk"], {checks: true,
                                              onToggle: (index, checked) => oui.status(\`\${index} \${checked}\`)});
oui.window("T", list);
list.checked = [false, true];
list.focus();
`, [" ", ""]);
    expect("the boxes", list.find("[x] milk") !== null, true);
    expect("ticked", list.find("[x] bread") !== null && list.row(13).includes("0 true"), true);
});

test("the file dialog asks for a file in a window, and on the terminal without one", async () => {
    const dialog = await pressing(`
oui.window("T", new oui.Label("body"));
oui.openFile({title: "Pick a file", filters: [{name: "Texts", patterns: "*.txt"}]})
    .then((path) => oui.status(path ?? "none"));
`, [""], {"notes.txt": "x"});
    expect("the dialog", dialog.find("Pick a file") !== null, true);
    expect("its file", dialog.find("notes.txt") !== null, true);
    const asked = await runScriptWith("const path = await oui.saveFile({title: \"Save as\"});\nconsole.log(\"got\", path);\n",
                                      "out.txt\n");
    expect("on the terminal", asked.output, "Save as: got out.txt\n");
});

test("grids, panels, docks and scroll views lay widgets out, and a widget taken out goes elsewhere", async () => {
    const shown = await pressing(`
const grid = new oui.Grid({columns: [8, "fill"]}).add(new oui.Label("Name:"), new oui.LineEdit({text: "Ana"}));
const moving = new oui.Label("moving");
const left = oui.column([moving]);
const right = oui.column([new oui.Label("right")]);
const dock = new oui.Dock().add("top", new oui.Label("top strip"))
    .add("fill", new oui.ScrollView(oui.column([grid, new oui.Panel(new oui.Label("in a panel"), {padding: 1}),
                                                oui.row([left, new oui.HSpacer(4), right])])));
oui.window("T", dock);
moving.remove();
right.add(moving);
`, [""]);
    for (const text of ["top strip", "Name:", "Ana", "in a panel"]) {
        expect(text, shown.find(text) !== null, true);
    }
    const right = shown.find("right");
    const moved = shown.find("moving");
    expect("under the other", moved !== null && right !== null && moved.row > right.row && moved.column === right.column,
           true);
});

test("a right click opens a widget's own menu, and a key handler hears keys first", async () => {
    const script = `
const field = new oui.LineEdit();
const seen = [];
field.onKey = (key, text) => { seen.push(key); oui.status(seen.join(" ")); return text === "x"; };
const label = new oui.Label("click me");
label.contextMenu = [{title: "&Hello", run: () => oui.status("hello")}];
oui.window("T", oui.column([label, field]));
field.focus();
`;
    const typed = await pressing(script, ["a", "x", "b", ""]);
    expect("the field", typed.find("ab") !== null && typed.find("axb") === null, true);
    expect("what it heard", typed.row(13).includes("A X B"), true);
    const menu = await pressing(script, ["\x1b[<2;3;1M", "\x1b[<2;3;1m", ""]);
    expect("the menu", menu.find("Hello") !== null, true);
    const chosen = await pressing(script, ["\x1b[<2;3;1M", "\x1b[<2;3;1m", "h", ""]);
    expect("what it ran", chosen.row(13).includes("hello"), true);
});

test("a theme changes the look, and widgets take colours by name", async () => {
    const screen = await inTerminal(`oui.theme("light");\nconst go = new oui.Button("Go");\n` +
        `oui.window("T", oui.column([new oui.LineEdit({text: "fieldtext"}), go]));\ngo.focus();`);
    expect("not Turbo's blue", sgrBefore(screen, "fieldtext") === "0;97;44", false);
    const named = await runScriptWith(`const label = new oui.Label("x");\nlabel.background = "table_background";\n` +
        `try { label.background = "no_such_colour"; } catch (error) { console.log(error.message); }\n`, "");
    expect("a colour that is not there", named.output, "there is no colour no_such_colour in the theme, nor is it one as \"#20a0ff\"\n");
});

test("a comparison, a folder view, a calculator and a terminal run in a script's window", async () => {
    const compared = await pressing(`
const comparison = new oui.Comparison({left: "same\\nleft only\\n", right: "same\\nright only\\n",
                                       leftTitle: "before", rightTitle: "after"});
oui.window("T", comparison);
oui.status(comparison.differences + " difference");
`, [""]);
    expect("the titles", compared.find("before") !== null && compared.find("after") !== null, true);
    expect("how many", compared.row(13).includes("1 difference"), true);
    const folder = await pressing(`oui.window("T", new oui.FolderView("photos"));`, [""],
                                  {"photos/one.jpg": "", "photos/two.jpg": ""});
    expect("the files", folder.find("one.jpg") !== null && folder.find("two.jpg") !== null, true);
    const calculated = await pressing(`const calculator = new oui.Calculator();\noui.window("T", calculator);\ncalculator.focus();`, ["6*7\r", ""]);
    expect("worked out", calculated.find("42") !== null, true);
    const terminal = await pressing(`oui.window("T", new oui.Terminal({command: ["sh", "-c", "echo from-the-shell; sleep 5"]}));`,
                                    ["", ""]);
    expect("its program's output", terminal.find("from-the-shell") !== null, true);
});

test("a dialog of the script's own gives back what closed it, and keeps its widgets to open again", async () => {
    const script = `
const name = new oui.LineEdit({onSubmit: (text) => dialog.close(text)});
const loud = new oui.CheckButton("Loud");
const dialog = new oui.Dialog("Who", oui.column([new oui.Label("Name:"), name, loud,
                                                 new oui.Button("OK", () => dialog.close(name.text + (loud.checked ? "!" : "")),
                                                                {default: true})]),
                              {width: 30});
let asked = 0;
const ask = async () => {
    ++asked;
    const answer = await dialog.show();
    oui.status(\`\${asked}: \${answer}\`);
};
oui.window("T", oui.column([new oui.Button("&Ask", ask)]));
oui.shortcut("Ctrl+T", ask);
`;
    const accepted = await pressing(script, ["\x14", "Ana", "\r", ""]);
    expect("accepted", accepted.row(13).includes("1: Ana"), true);
    const cancelled = await pressing(script, ["\x14", "Ana", "\x1b", ""]);
    expect("cancelled", cancelled.row(13).includes("1: null"), true);
    const again = await pressing(script, ["\x14", "Ana", "\r", "\x14", ""]);
    expect("open again", again.find("Who") !== null && again.find("Ana") !== null, true);
    const twice = await pressing(script, ["\x14", "Ana", "\r", "\x14", "\r", ""]);
    expect("what it kept", twice.row(13).includes("2: Ana"), true);
    // From the box, which leaves Enter alone, to the default button.
    const button = await pressing(script, ["\x14", "Ana", "\t", " ", "\r", ""]);
    expect("the default button", button.row(13).includes("1: Ana!"), true);
});

test("a description's Window is a dialog too, with its title and its width, kept to open again", async () => {
    const script = `
const ui = oui.uiFromFile("ui/rename.oui");
const dialog = new oui.Dialog(ui);
ui.ok.onClick = () => dialog.close(ui.name.text);
const ask = async () => oui.status(\`got \${await dialog.show()}\`);
oui.window("T", new oui.Label("main"));
oui.shortcut("Ctrl+T", ask);
`;
    const files = {"ui/rename.oui": `Window "Rename" width=40\n  Label "New name:"\n  LineEdit id=name\n` +
                                    `  Button "OK" id=ok default\n`};
    const open = await pressing(script, ["\x14", "Bea", ""], files);
    const title = open.find("Rename");
    expect("its title", title !== null, true);
    // Forty columns from one side of its frame to the other, in a terminal of sixty.
    expect("its width", open.row(title.row).trim().length, 40);
    const again = await pressing(script, ["\x14", "Bea", "\t", "\r", "\x14", "\t", "\r", ""], files);
    expect("kept", again.row(13).includes("got Bea"), true);
    const wrong = await runScriptWith(`try { new oui.Dialog(oui.ui("Column\\n  Label \\"x\\"\\n")); } ` +
                                      `catch (error) { console.log(error.message); }\n`, "");
    expect("not a Window", wrong.output.startsWith("oui.Dialog: the description starts with a Column"), true);
});

test("a text edit finds, replaces, selects, undoes and works on lines, its places counted as JavaScript counts", async () => {
    const {output} = await runScript(`
const edit = new oui.TextEdit({text: "uno dos\\ntres ñ 😀 dos\\n"});
const say = (...values) => console.log(values.map((value) => JSON.stringify(value)).join(" "));
say(edit.find("dos"), edit.selection, edit.selectedText, edit.line, edit.column);
say(edit.find("dos"), edit.selection, edit.line, edit.column);
say(edit.find("DOS"), edit.find("DOS", {matchCase: false}), edit.find("d.s", {regex: true}));
edit.select(15, 17);
say(edit.selectedText);
edit.cursor = 12;
say(edit.cursor, edit.line, edit.column);
say(edit.modified, edit.replaceAll("dos", "2"), edit.text, edit.modified);
say(edit.undo(), edit.text);
edit.modified = false;
edit.select(0);
edit.insert("¡");
say(edit.text.split("\\n")[0], edit.cursor, edit.modified);
say(edit.goToLine(2), edit.line, edit.goToLine(9));
say(edit.duplicateLines(), edit.text);
say(edit.toggleComment("//"), edit.text.split("\\n")[2]);
say(edit.moveLines("up"), edit.text.split("\\n")[1]);
say(edit.deleteLines(), edit.text);
edit.selectAll();
say(edit.selectedText.length === edit.text.length);
`);
    expect("output", output, [
        `true [4,7] "dos" 1 8`,
        `true [18,21] 2 14`,
        `false true true`,
        `"😀"`,
        `12 2 5`,
        `false 2 "uno 2\\ntres ñ 😀 2\\n" true`,
        `true "uno dos\\ntres ñ 😀 dos\\n"`,
        `"¡uno dos" 1 true`,
        `true 2 false`,
        `true "¡uno dos\\ntres ñ 😀 dos\\ntres ñ 😀 dos\\n"`,
        `true "// tres ñ 😀 dos"`,
        `true "// tres ñ 😀 dos"`,
        `true "¡uno dos\\ntres ñ 😀 dos\\n"`,
        `true`,
        ``].join("\n"));
});

test("a text edit colours its text by rules as OUI Code's languages write them, and says where rules are wrong", async () => {
    const screen = await inTerminal(`const edit = new oui.TextEdit({text: "if xvalue"});\n` +
        `edit.highlight = {start: [{match: oui.words(["if"]), kind: "keyword"}]};\n` +
        `const go = new oui.Button("Go");\noui.window("T", oui.column([oui.fill(edit), go]));\ngo.focus();`);
    expect("coloured apart", sgrBefore(screen, "if") !== undefined && sgrBefore(screen, "if") !== sgrBefore(screen, " xvalue"),
           true);
    const plain = await inTerminal(`const edit = new oui.TextEdit({text: "if xvalue"});\n` +
        `const go = new oui.Button("Go");\noui.window("T", oui.column([oui.fill(edit), go]));\ngo.focus();`);
    expect("in one colour without", sgrBefore(plain, "if xvalue") !== undefined, true);
    const {output} = await runScript(`try { new oui.TextEdit().highlight = {start: [{match: /x/, kind: "nope"}]}; } ` +
        `catch (error) { console.log(error.message); }\n`);
    expect("wrong", output, `highlight: state start, rule 1: unknown kind "nope"\n`);
});

test("a field selects, writes where it is told and changes what it shows while empty", async () => {
    const {output} = await runScript(`
const field = new oui.LineEdit({text: "hola ñandú", placeholder: "Name"});
field.select(5, 10);
console.log(field.selectedText, JSON.stringify(field.selection), field.placeholder);
field.insert("mundo");
field.placeholder = "Who";
console.log(field.text, field.placeholder);
field.selectAll();
console.log(field.selectedText);
`);
    expect("output", output, `ñandú [5,10] Name\nhola mundo Who\nhola mundo\n`);
    const shown = await pressing(`const field = new oui.LineEdit({placeholder: "Name"});\n` +
                                 `oui.window("T", oui.column([field, new oui.Button("Go")]));\nfield.placeholder = "Shown later";\n`,
                                 [""]);
    expect("placeholder", shown.find("Shown later") !== null && shown.find("Name") === null, true);
});

test("tabs give their pages, retitle, move and take them out, and close a page from its mark", async () => {
    const {output} = await runScript(`
const [a, b, c] = ["A", "B", "C"].map((text) => new oui.Label(text));
const tabs = new oui.Tabs().add("One", a).add("Two", b).add("Three", c);
tabs.setTitle(1, "Second");
tabs.move(0, 2);
tabs.marked = [true, false, false];
console.log(tabs.pages.map((page) => page.text).join(), [0, 1, 2].map((index) => tabs.title(index)).join(),
            tabs.marked.join());
const taken = tabs.remove(0);
console.log(taken.text, tabs.count, tabs.pages.map((page) => page.text).join(), tabs.marked.join());
oui.column([taken]);
console.log("placed again");
`);
    expect("output", output, "B,C,A Second,Three,One true,false,false\nB 2 C,A false,false\nplaced again\n");
    const script = `
const tabs = new oui.Tabs().add("One", new oui.Label("first page")).add("Two", new oui.Label("second page"));
oui.window("T", tabs);
`;
    const before = await pressing(script, [""]);
    const mark = before.find("×");
    expect("a close mark", mark !== null, true);
    const closed = await pressing(script, [`\x1b[<0;${mark.column + 1};${mark.row + 1}M`, `\x1b[<0;${mark.column + 1};${mark.row + 1}m`, ""]);
    expect("closed", closed.find("One") === null && closed.find("Two") !== null, true);
    const kept = await pressing(script + `tabs.onClose = (index) => oui.status("asked " + index);\n`,
                                [`\x1b[<0;${mark.column + 1};${mark.row + 1}M`, `\x1b[<0;${mark.column + 1};${mark.row + 1}m`, ""]);
    expect("kept", kept.find("One") !== null && kept.row(13).includes("asked 0"), true);
});

test("a table's columns take the widths given, and its headings can hide", async () => {
    const {output} = await runScript(`
const table = new oui.Table(["Name", "Size"], {widths: [10]});
table.widths = [12, 6];
table.stretch = null;
table.resizable = true;
console.log(JSON.stringify(table.widths), table.stretch, table.resizable, table.headings);
`);
    expect("output", output, "[12,6] null true true\n");
    const hidden = await pressing(`const table = new oui.Table(["Heading"], {rows: [["row one"]]});\n` +
                                  `table.headings = false;\noui.window("T", table);`, [""]);
    expect("no heading", hidden.find("Heading") === null && hidden.find("row one") !== null, true);
});

test("a canvas shows what is drawn on it, says its size, and where the pointer is", async () => {
    const script = `
const canvas = new oui.Canvas({
    onResize: (width, height) => {
        canvas.clear("#000000").fill(2, 1, 4, 2, "#ff0000").text(1, 4, \`\${width}x\${height}\`, {color: "#ffff00", bold: true});
    },
    onMouse: (mouse) => { if (mouse.kind === "press") oui.status(\`at \${mouse.x},\${mouse.y} \${mouse.clicks}\`); },
});
oui.window("T", oui.column([new oui.Label("above"), oui.fill(canvas)]));
canvas.focus();
`;
    const drawn = await pressing(script, [""]);
    // Below the label and the row between them: all the width, and the height left, with no status line yet.
    const size = drawn.find("60x12");
    expect("its size", size !== null, true);
    const clicked = await pressing(script, ["\x1b[<0;6;4M", "\x1b[<0;6;4m", ""]);
    expect("where", clicked.row(13).includes("at 5,1 1"), true);
    const screen = await inTerminal(`const canvas = new oui.Canvas({width: 20, height: 5});\n` +
        `canvas.text(0, 0, "drawnhere", {color: "#ffff00", background: "#0000aa"});\n` +
        `oui.window("T", oui.column([canvas]));`);
    expect("in its colours", sgrBefore(screen, "drawnhere"), "0;93;44");
});

test("a script's settings last from one run to the next, kept as JSON by name", async () => {
    const folder = await scratch();
    const run = async (script) => {
        oui.writeFile(`${folder}/keep.js`, script);
        return (await capture(`XDG_CONFIG_HOME=${quote(`${folder}/config`)} ${quote(ouiRun)} keep.js </dev/null`,
                              {cwd: folder})).output;
    };
    const first = await run(`console.log(JSON.stringify(oui.settings.get("size", [80, 24])));\n` +
                            `oui.settings.set("size", [100, 30]);\noui.settings.set("name", "ñandú = sí");\n` +
                            `try { oui.settings.set("a=b", 1); } catch (error) { console.log(error.message); }\n`);
    const second = await run(`console.log(JSON.stringify(oui.settings.get("size")), oui.settings.get("name"));\n` +
                             `oui.settings.remove("name");\n`);
    const third = await run(`console.log(oui.settings.get("name", "gone"));\n`);
    const file = oui.readFile(`${folder}/config/oui_run/keep.ini`);
    await capture(`rm -rf ${quote(folder)}`);
    expect("first", first, "[80,24]\n\"a=b\" cannot be a setting's name: one with no =, no line break, and no blank or # at its start\n");
    expect("second", second, "[100,30] ñandú = sí\n");
    expect("third", third, "gone\n");
    expect("the file", file, "size=[100,30]\n");
});

test("a theme that is not there is told with those the build has", async () => {
    const {output} = await runScript(`try { oui.theme("nope"); } catch (error) { console.log(error.message); }\n`);
    // classic is the desktop window's, and only a build with windows has it.
    const themes = ouiRun.includes("gui") ? "turbo, night, light, monochrome, amber, phosphor, commander or classic" :
        "turbo, night, light, monochrome, amber, phosphor or commander";
    expect("output", output, `there is no theme nope: ${themes}\n`);
});

test("widgets take fixed colours besides the theme's", async () => {
    const {output} = await runScript(`const label = new oui.Label("x");\nlabel.background = "#20a0ff";\nlabel.color = "#ffffff";\n` +
        `try { label.color = "#12345"; } catch (error) { console.log(error.message); }\n`);
    expect("output", output, `there is no colour #12345 in the theme, nor is it one as "#20a0ff"\n`);
});

test("a Markdown view says where a link leads out, goes itself to a section, and to one it is told", async () => {
    const lines = Array.from({length: 30}, (_, index) => `filler ${index}`).join("\n\n");
    const script = `
const view = new oui.MarkdownView(${JSON.stringify(`[outward](https://example.org) [inward](#far-away)

${lines}

## Far away

the end`)},
                                  {onLink: (target) => oui.status("link " + target)});
oui.window("T", view);
oui.shortcut("Ctrl+T", () => oui.status("went " + view.goTo("#far-away") + " " + view.goTo("nowhere")));
`;
    const start = await pressing(script, [""]);
    const outward = start.find("outward");
    const inward = start.find("inward");
    const click = (at) => [`\x1b[<0;${at.column + 1};${at.row + 1}M`, `\x1b[<0;${at.column + 1};${at.row + 1}m`];
    const out = await pressing(script, [...click(outward), ""]);
    expect("told", out.row(13).includes("link https://example.org"), true);
    const within = await pressing(script, [...click(inward), ""]);
    expect("went itself", within.find("the end") !== null && !within.row(13).includes("link"), true);
    const told = await pressing(script, ["\x14", ""]);
    // At the end of the text, as far as it scrolls, under the status line that came with the answer.
    expect("went when told", told.find("Far away") !== null && told.row(13).includes("went true false"), true);
});

test("the clipboard keeps what is copied, and openExternally hands a link to the system", async () => {
    const folder = await scratch();
    oui.writeFile(`${folder}/main.js`, `oui.copy("hola ñ");\nconsole.log(oui.paste());\n` +
        `console.log(oui.openExternally("https://example.org"));\n`);
    oui.writeFile(`${folder}/xdg-open`, `#!/bin/sh\nprintf %s "$1" > '${folder}/told'\n`);
    await capture(`chmod +x ${quote(`${folder}/xdg-open`)}`);
    // No display, so the clipboard is the program's own and not the desktop's.
    const {output} = await capture(`env -u DISPLAY -u WAYLAND_DISPLAY PATH=${quote(folder)}:"$PATH" ${quote(ouiRun)} main.js`,
                                   {cwd: folder});
    await capture("sleep 0.3");
    const told = oui.readFile(`${folder}/told`);
    await capture(`rm -rf ${quote(folder)}`);
    expect("pasted", output, "hola ñ\ntrue\n");
    expect("handed over", told, "https://example.org");
});

// The programs in examples/oui_run/programs, each in a terminal, keys pressed as a person would.
const programs = `${root}/examples/oui_run/programs`;

test("2048 slides and joins the tiles with the arrows, and scores what it joins", async () => {
    const here = await place(ouiRun, {});
    const start = await here.run([`${programs}/2048.js`, "7"], [""], {columns: 60, rows: 18});
    // The same game for the same seed: left, then up, joins two 2s and two 4s.
    const moved = await here.run([`${programs}/2048.js`, "7"], ["\x1b[D", "\x1b[A", ""], {columns: 60, rows: 18});
    await here.remove();
    expect("a new game", start.find("Score: 0") !== null, true);
    expect("after two moves", moved.find("Score: 8") !== null, true);
    expect("the 8 they made", moved.row(0).trim().startsWith("8"), true);
});

test("processes lists the programs running, the filter keeps those it names, a heading orders them", async () => {
    const here = await place(ouiRun, {});
    // oui_run itself is always one of them while this runs.
    const shown = await here.run([`${programs}/processes.js`], ["oui_run", ""], {columns: 90, rows: 20});
    // The heading of names, on the second row from column 25, clicked.
    const click = [`\x1b[<0;30;2M`, `\x1b[<0;30;2m`];
    const byName = await here.run([`${programs}/processes.js`], [...click, ""], {columns: 90, rows: 20});
    await here.remove();
    const rows = Array.from({length: 20}, (_, index) => shown.row(index));
    expect("busiest first", shown.find("CPU % ▼") !== null, true);
    expect("oui_run in it", rows.some((row) => /^ *\d+ +[\d.]+ +[\d.]+ +oui_run/.test(row)), true);
    expect("how many", rows.some((row) => /^\d+ of \d+ programs\./.test(row.trim())), true);
    expect("by name", byName.find("Program ▲") !== null && byName.find("CPU % ▼") === null, true);
    const names = Array.from({length: 15}, (_, index) => byName.row(index + 2).slice(25).trim()).filter(Boolean);
    expect("in order", JSON.stringify(names), JSON.stringify([...names].sort((a, b) => a.localeCompare(b))));
});

test("files lists a folder, shows the file chosen, and goes in and up", async () => {
    const files = {"notes/readme.md": "# Notes\n\nSome **bold** text.\n", "notes/code.js": "const a = 1;\n",
                   "notes/sub/x.txt": "x\n"};
    const here = await place(ouiRun, files);
    // Down to readme.md, past ../, sub/ and code.js.
    const markdown = await here.run([`${programs}/files.js`, "notes"], ["\x1b[B", "\x1b[B", "\x1b[B", ""],
                                    {columns: 70, rows: 10});
    const code = await here.run([`${programs}/files.js`, "notes"], ["\x1b[B", "\x1b[B", ""], {columns: 70, rows: 10});
    const inside = await here.run([`${programs}/files.js`, "notes"], ["\x1b[B", "\r", ""], {columns: 70, rows: 10});
    const up = await here.run([`${programs}/files.js`, "notes"], ["\x1b[B", "\r", "\x7f", ""], {columns: 70, rows: 10});
    await here.remove();
    expect("Markdown as it reads", markdown.find("Some bold text.") !== null, true);
    expect("code as it is", code.find("const a = 1;") !== null, true);
    expect("into sub", inside.row(0).trim(), "notes/sub");
    expect("its file", inside.find("x.txt") !== null, true);
    expect("back up", up.row(0).trim(), "notes");
});

test("todo adds what is typed, marks it done, and keeps the list for next time", async () => {
    const here = await place(ouiRun, {});
    const shown = await here.run([`${programs}/todo.js`], ["bread", "\r", "call Ana", "\r", "\x1bd", ""],
                                 {columns: 70, rows: 12});
    const kept = JSON.parse(oui.readFile(`${here.folder}/todo.json`));
    const again = await here.run([`${programs}/todo.js`], [""], {columns: 70, rows: 12});
    // A single click on bread's box, on the second row, under the field.
    const clicked = await here.run([`${programs}/todo.js`], ["\x1b[<0;2;2M", "\x1b[<0;2;2m", ""],
                                   {columns: 70, rows: 12});
    const after = JSON.parse(oui.readFile(`${here.folder}/todo.json`));
    await here.remove();
    expect("a click on its box", clicked.find("[x] bread") !== null && after[0].done, true);
    expect("the list", shown.find("[ ] bread") !== null && shown.find("[x] call Ana") !== null, true);
    expect("what is left", shown.find("1 of 2 left to do.") !== null, true);
    expect("the file", JSON.stringify(kept), JSON.stringify([{text: "bread", done: false}, {text: "call Ana", done: true}]));
    expect("next time", again.find("[x] call Ana") !== null, true);
});

test("trim_trailing_spaces in a dry run says what it would do and writes nothing", async () => {
    const {code, output, after} = await recipe("", [], {driver: calling("trim_trailing_spaces.js",
        `script.processFile("src/main.js", true);`)});
    expect("code", code, 0);
    expect("output", output, "Would tidy 2 lines.\n");
    expect("src/main.js", after["src/main.js"], fixture["src/main.js"]);
});

test("trim_trailing_spaces for real tidies the file", async () => {
    const {code, after} = await recipe("", [], {driver: calling("trim_trailing_spaces.js",
        `script.processFile("src/main.js", false);`)});
    expect("code", code, 0);
    expect("src/main.js", after["src/main.js"],
        "// TODO: read the settings\nconst colour = \"red\";\nconsole.log(colour);\n/* FIXME: colour by name */\n");
});

test("markdown_toc puts a table of contents under the title, and replaces it after", async () => {
    const {code, output} = await recipe("", [], {driver: calling("markdown_toc.js",
        `const once = script.transform(oui.readFile("notes/intro.md"));\n` +
        `console.log(once);\nconsole.log(script.transform(once) === once);`)});
    expect("code", code, 0);
    expect("output", output,
        "# Intro\n\n<!-- toc -->\n- [Getting started](#getting-started)\n- [Getting started](#getting-started-1)\n" +
        "  - [Details](#details)\n<!-- /toc -->\n\nSee [the guide](guide.md) and [setup](setup.md#start).\n\n" +
        "## Getting started\n\nTODO: write this part\n\n## Getting started\n\n```sh\n# not a heading\n```\n\n" +
        "### Details\n\ntrue\n");
});

test("check_links finds the links that lead nowhere", async () => {
    const {code, output} = await recipe("", [], {driver: calling("check_links.js",
        `script.process(true, ".");`)});
    expect("code", code, 0);
    expect("output", output, "notes/intro.md:3: setup.md#start\n1 links lead nowhere.\n");
});

await run(cases, oui.args[1]);
