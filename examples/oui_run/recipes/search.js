// Finds a regular expression in every text file of a folder, as grep -rn does, and lists each line it is on:
// oui_run search.js PATTERN [FOLDER] [--include=*.js,*.h] [--ignore-case]
// Ends with 1 when nothing matched, as grep does, so a shell script can tell.
//
//     oui_run search.js "TODO|FIXME" src --include=*.cpp,*.h
import {matcher, parseArgs, readText, walk} from "./lib/files.js";

const {options, positional} = parseArgs(oui.args);
const [pattern, folder = "."] = positional;
if (!pattern) {
    console.log("usage: oui_run search.js PATTERN [FOLDER] [--include=PATTERNS] [--ignore-case]");
    oui.exit(2);
}

let expression;
try {
    expression = new RegExp(pattern, options["ignore-case"] ? "i" : "");
} catch (error) {
    console.log(`Not a regular expression: ${error.message}`);
    oui.exit(2);
}

const wanted = matcher(options.include);
let matches = 0;
let files = 0;
for (const file of walk(folder).filter(wanted)) {
    const text = readText(file);
    if (text === null) {
        continue;
    }
    let found = false;
    text.split("\n").forEach((line, index) => {
        if (expression.test(line)) {
            console.log(`${file}:${index + 1}: ${line.trim()}`);
            matches += 1;
            found = true;
        }
    });
    files += found ? 1 : 0;
}
console.log(matches === 0 ? "No matches." : `${matches} matches in ${files} files.`);
oui.exit(matches === 0 ? 1 : 0);
