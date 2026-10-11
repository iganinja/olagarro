// For OUI Code's Tools > Run Script, on the whole project: finds the links in its Markdown files that point to a
// file that is not there.
//
// A script that exports process works on its own, given the project's folder. It only reads, so a dry run and one for
// real do the same; a script that changes things should change nothing when dryRun is true, and say what it would do.

// Every Markdown file under folder, but in the folders nobody means to look at.
function markdownFiles(folder, found = []) {
    for (const name of oui.listDir(folder)) {
        if (name.endsWith("/")) {
            if (![".git/", "node_modules/", "build/"].includes(name)) {
                markdownFiles(`${folder}/${name.slice(0, -1)}`, found);
            }
        } else if (name.endsWith(".md")) {
            found.push(`${folder}/${name}`);
        }
    }
    return found;
}

export function process(dryRun, folder) {
    let broken = 0;
    for (const file of markdownFiles(folder)) {
        const here = file.slice(0, file.lastIndexOf("/"));
        oui.readFile(file).split("\n").forEach((line, index) => {
            // [text](target), but not images' ![text](target) nor web addresses, nor anchors in the same file.
            for (const [, target] of line.matchAll(/(?<!!)\[[^\]]*\]\(([^)\s]+)\)/g)) {
                if (/^[a-z]+:/i.test(target) || target.startsWith("#")) {
                    continue;
                }
                const path = target.replace(/#.*$/, "");
                const whole = path.startsWith("/") ? `${folder}${path}` : `${here}/${path}`;
                if (!oui.exists(whole)) {
                    console.log(`${file.slice(folder.length + 1)}:${index + 1}: ${target}`);
                    broken += 1;
                }
            }
        });
    }
    console.log(broken === 0 ? "Every link leads somewhere." : `${broken} links lead nowhere.`);
}
