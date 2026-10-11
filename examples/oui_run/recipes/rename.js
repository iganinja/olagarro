// Renames the files in a folder whose names match a regular expression: oui_run rename.js PATTERN REPLACEMENT
// [FOLDER] [--write]
// Without --write it changes nothing and lists what it would rename. A name another file has already, or that two
// files would end up with, stops it before it renames anything. Only the folder itself, not those inside it.
// rename_window.js does the same in a window.
//
//     oui_run rename.js "^IMG_(\d+)\.JPG$" "photo_$1.jpg" photos --write
//
// oui has no call to rename a file: a command does it, `mv` here, as for anything else oui does not have. Names go
// to the shell through quote(), whatever is in them; on Windows, `move` and its own quoting would do.
import {join, parseArgs, quote} from "./lib/files.js";
import {planRenames} from "./lib/rename.js";

const {options, positional} = parseArgs(oui.args);
const [pattern, replacement, folder = "."] = positional;
if (pattern === undefined || replacement === undefined) {
    console.log("usage: oui_run rename.js PATTERN REPLACEMENT [FOLDER] [--write]");
    oui.exit(2);
}
const {renames, problems} = planRenames(folder, new RegExp(pattern), replacement);
if (problems.length > 0) {
    console.log(problems.join("\n"));
    oui.exit(1);
}

for (const {from, to} of renames) {
    console.log(`${from} -> ${to}`);
}
if (renames.length === 0) {
    console.log("Nothing to rename.");
} else if (!options.write) {
    console.log(`Would rename ${renames.length} files. Again with --write to do it.`);
} else {
    for (const {from, to} of renames) {
        const code = await oui.run(`mv -- ${quote(join(folder, from))} ${quote(join(folder, to))}`);
        if (code !== 0) {
            console.log(`Could not rename ${from}; stopped there.`);
            oui.exit(1);
        }
    }
    console.log(`Renamed ${renames.length} files.`);
}
