// Lines, words and characters of files, as wc counts them, with a total: oui_run count.js FILE_OR_FOLDER...
// A folder counts every text file in it, binary ones left out. With --include=*.js,*.h only files with those
// names.
//
//     oui_run count.js src --include=*.cpp,*.h
import {matcher, parseArgs, readText, table, walk} from "./lib/files.js";

const {options, positional} = parseArgs(oui.args);
if (positional.length === 0) {
    console.log("usage: oui_run count.js FILE_OR_FOLDER... [--include=PATTERNS]");
    oui.exit(2);
}

const wanted = matcher(options.include);
// A folder lists what is in it; a file, nothing.
const files = positional.flatMap((path) => (oui.listDir(path).length > 0 ? walk(path) : [path])).filter(wanted);

const rows = [];
const total = {lines: 0, words: 0, characters: 0};
for (const file of files) {
    const text = readText(file);
    if (text === null) {
        continue;
    }
    // A last line without its newline still counts, as an editor shows it.
    const lines = text === "" ? 0 : text.split("\n").length - (text.endsWith("\n") ? 1 : 0);
    const words = text.split(/\s+/).filter(Boolean).length;
    // Characters, not UTF-16 code units: [...text] goes by code point, so "ñ" and an emoji count one each.
    const characters = [...text].length;
    rows.push([lines, words, characters, file]);
    total.lines += lines;
    total.words += words;
    total.characters += characters;
}
if (rows.length > 1) {
    rows.push([total.lines, total.words, total.characters, "total"]);
}
console.log(table([["lines", "words", "chars", "file"], ...rows]));
