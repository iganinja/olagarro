// A Markdown report of the TODO, FIXME, HACK and XXX comments in a folder, grouped by tag:
// oui_run todo_report.js [FOLDER] [REPORT.md]
// Without REPORT.md the report goes to the terminal.
//
//     oui_run todo_report.js src todo.md
import {readText, walk} from "./lib/files.js";

const [folder = ".", report] = oui.args;
const tags = ["FIXME", "TODO", "HACK", "XXX"];
// A tag as a word, with a colon or not after it, and what follows it on the line.
const expression = new RegExp(`\\b(${tags.join("|")})\\b:?\\s*(.*)`);

const found = new Map(tags.map((tag) => [tag, []]));
for (const file of walk(folder)) {
    // The report itself is not searched, from an earlier run.
    if (file === report) {
        continue;
    }
    const text = readText(file);
    if (text === null) {
        continue;
    }
    text.split("\n").forEach((line, index) => {
        const match = expression.exec(line);
        if (match) {
            // The end of a block comment is not part of what it says.
            found.get(match[1]).push({file, line: index + 1, text: match[2].replace(/\s*\*\/\s*$/, "").trim()});
        }
    });
}

const total = [...found.values()].reduce((sum, list) => sum + list.length, 0);
const lines = [`# Things to do in ${folder}`, "", `${total} in all.`];
for (const [tag, list] of found) {
    if (list.length > 0) {
        lines.push("", `## ${tag} (${list.length})`, "");
        lines.push(...list.map((each) => `- [${each.file}:${each.line}](${each.file}#L${each.line}) ${each.text}`));
    }
}
const markdown = lines.join("\n") + "\n";

if (report === undefined) {
    console.log(markdown.trimEnd());
} else if (oui.writeFile(report, markdown)) {
    console.log(`${report}: ${total} things to do`);
} else {
    console.log(`Could not write ${report}`);
    oui.exit(1);
}
