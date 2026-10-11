// Folders and what is in their files: oui_run files.js [FOLDER], or oui_run_gui --window files.js [FOLDER]
// The list on the left is the folder, its folders first; the file chosen in it shows on the right, Markdown as it
// reads and any other text as it is written. Enter goes into a folder, and into ".." up; Backspace goes up too.
// Escape ends the program.
import {join, readText} from "../recipes/lib/files.js";

// What is shown of a file at most: enough to see what it is.
const most = {lines: 400, bytes: 64 * 1024};

const where = new oui.Label("", {bold: true});
const entries = new oui.List();
const preview = new oui.MarkdownView();
let folder = oui.args[0] ?? ".";
let names = [];

// The folder above: a name less, or one more ".." when there is no name left to take.
function parent(path) {
    const parts = path.split("/").filter((part) => part !== "" && part !== ".");
    if (parts.length === 0 || parts.at(-1) === "..") {
        return [...parts, ".."].join("/");
    }
    parts.pop();
    return parts.length === 0 ? "." : (path.startsWith("/") ? "/" : "") + parts.join("/");
}

function open(path) {
    const listed = oui.listDir(path);
    folder = path;
    // Folders first, each part in name order; ".." on top, to go up.
    const folders = listed.filter((name) => name.endsWith("/")).sort();
    const files = listed.filter((name) => !name.endsWith("/")).sort();
    names = ["../", ...folders, ...files];
    entries.items = names;
    entries.selected = 0;
    where.text = folder;
    show();
}

function show() {
    const name = names[entries.selected];
    if (name === undefined || name.endsWith("/")) {
        preview.markdown = "";
        return;
    }
    const path = join(folder, name);
    const text = readText(path);
    if (text === null) {
        const bytes = oui.readFile(path);
        preview.markdown = bytes === null ? `*${name} cannot be read.*` : `*A binary file, ${bytes.length} bytes.*`;
        return;
    }
    let shown = text.length > most.bytes ? text.slice(0, most.bytes) : text;
    const lines = shown.split("\n");
    const cut = lines.length > most.lines || shown.length < text.length;
    shown = lines.slice(0, most.lines).join("\n");
    if (/\.(md|markdown)$/i.test(name)) {
        preview.markdown = shown + (cut ? "\n\n*…*" : "");
        return;
    }
    // Text as it is written, as code: its own fences cannot close the one around it.
    const fenced = shown.split("\n").map((line) => (line.startsWith("```") ? ` ${line}` : line)).join("\n");
    preview.markdown = `\`\`\`\n${fenced}\n\`\`\`` + (cut ? "\n\n*…*" : "");
}

function enter() {
    const name = names[entries.selected];
    if (name === "../") {
        open(parent(folder));
    } else if (name?.endsWith("/")) {
        open(join(folder, name.slice(0, -1)));
    }
}

entries.onSelect = show;
entries.onActivate = enter;
oui.shortcut("Backspace", () => open(parent(folder)));
oui.shortcut("Escape", () => oui.quit());
open(folder);
oui.window("Files", oui.column([where, oui.fill(new oui.Split(entries, preview, {ratio: 0.35}))]));
entries.focus();
