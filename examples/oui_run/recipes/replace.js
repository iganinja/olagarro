// Replaces a regular expression in every text file of a folder: oui_run replace.js PATTERN REPLACEMENT [FOLDER]
// [--include=*.js,*.h] [--write]
// Without --write it changes nothing and shows each line as it is and as it would be. With it, it asks first, then
// writes. The replacement can use what the pattern captured, as JavaScript's replace does: $1, $2, $<name>.
//
//     oui_run replace.js "colour" "color" docs --include=*.md
//     oui_run replace.js "(\w+)_(\w+)\(" "$1$2(" src --write
import {matcher, parseArgs, readText, walk} from "./lib/files.js";

const {options, positional} = parseArgs(oui.args);
const [pattern, replacement, folder = "."] = positional;
if (pattern === undefined || replacement === undefined) {
    console.log("usage: oui_run replace.js PATTERN REPLACEMENT [FOLDER] [--include=PATTERNS] [--write]");
    oui.exit(2);
}
const expression = new RegExp(pattern, "g");

// Each file that would change, with its new text and how many places change in it.
const changes = [];
for (const file of walk(folder).filter(matcher(options.include))) {
    const text = readText(file);
    if (text === null) {
        continue;
    }
    const replaced = text.replace(expression, replacement);
    if (replaced === text) {
        continue;
    }
    const before = text.split("\n");
    const after = replaced.split("\n");
    // A replacement may add or take away lines; only line by line is shown then.
    if (before.length === after.length) {
        before.forEach((line, index) => {
            if (line !== after[index]) {
                console.log(`${file}:${index + 1}\n  - ${line.trim()}\n  + ${after[index].trim()}`);
            }
        });
    } else {
        console.log(`${file}: the lines change, from ${before.length} to ${after.length}`);
    }
    changes.push({file, replaced, count: text.match(expression).length});
}

const total = changes.reduce((sum, change) => sum + change.count, 0);
if (changes.length === 0) {
    console.log("Nothing to replace.");
} else if (!options.write) {
    console.log(`Would replace ${total} in ${changes.length} files. Again with --write to do it.`);
} else if (await oui.confirm(`Replace ${total} in ${changes.length} files?`)) {
    const failed = changes.filter((change) => !oui.writeFile(change.file, change.replaced));
    for (const {file} of failed) {
        console.log(`Could not write ${file}`);
    }
    console.log(`Replaced in ${changes.length - failed.length} files.`);
    oui.exit(failed.length === 0 ? 0 : 1);
} else {
    console.log("Nothing changed.");
}
