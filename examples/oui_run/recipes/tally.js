// Runs a command, takes its output in, and counts how many times each line comes, most first:
// oui_run tally.js COMMAND [--top=N]
// What a command prints is text to work on like any other; this is the way to get at it.
//
//     oui_run tally.js "git log --format=%an"          commits by author
//     oui_run tally.js "git log --format=%ad --date=format:%A"   commits by day of the week
import {parseArgs, table} from "./lib/files.js";

const {options, positional} = parseArgs(oui.args);
const [command] = positional;
if (!command) {
    console.log("usage: oui_run tally.js COMMAND [--top=N]");
    oui.exit(2);
}

// output takes what the command prints as it comes, in pieces that need not end at a line: kept whole, and split
// once it is all there. Its errors come the same way, so a failed command's message is shown as it is.
let printed = "";
const code = await oui.run(command, {output: (text) => { printed += text; }});
if (code !== 0) {
    console.log(`"${command}" ended with ${code}:\n${printed.trimEnd()}`);
    oui.exit(1);
}

const counts = new Map();
for (const line of printed.split("\n").map((each) => each.trim()).filter(Boolean)) {
    counts.set(line, (counts.get(line) ?? 0) + 1);
}
const top = Number(options.top ?? 20);
const sorted = [...counts].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0])).slice(0, top);
const total = [...counts.values()].reduce((sum, count) => sum + count, 0);
console.log(table([["count", "%", "line"], ...sorted.map(([line, count]) =>
    [count, Number((100 * count / total).toFixed(1)), line])]));
