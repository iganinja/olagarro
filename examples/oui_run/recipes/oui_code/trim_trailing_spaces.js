// For OUI Code's Tools > Run Script, on the document: takes away the spaces and tabs at the ends of lines, and
// leaves the file ending in exactly one line break.
//
// A script that exports processFile works on one file. F5 runs it as a dry run, on a copy, and shows the copy as it
// leaves it; Shift+F5 runs it for real, on the document, saved first.

export function processFile(file, dryRun) {
    const text = oui.readFile(file);
    const tidy = text.replace(/[ \t]+$/gm, "").replace(/\n*$/, "\n");
    if (tidy === text) {
        console.log("Nothing to tidy.");
        return;
    }
    const lines = text.split("\n").filter((line) => /[ \t]$/.test(line)).length;
    console.log(`${dryRun ? "Would tidy" : "Tidied"} ${lines} lines.`);
    // A dry run works on a copy, so writing is safe; leaving it out keeps a dry run from writing anywhere at all.
    if (!dryRun) {
        oui.writeFile(file, tidy);
    }
}
