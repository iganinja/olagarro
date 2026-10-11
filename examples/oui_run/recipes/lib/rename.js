// What renaming the files in a folder whose names match a regular expression would do, for rename.js and
// rename_window.js. Only the folder itself, not those inside it.

// Each file that changes, from and to, and what stops it all: a new name another file has already, or that two
// files would end up with.
export function planRenames(folder, expression, replacement) {
    const names = oui.listDir(folder).filter((name) => !name.endsWith("/"));
    const renames = names
        .filter((name) => expression.test(name))
        .map((name) => ({from: name, to: name.replace(expression, replacement)}))
        .filter(({from, to}) => from !== to);
    // Every name there now, and each new one as it is given out.
    const taken = new Set(names);
    const problems = [];
    for (const {from, to} of renames) {
        if (taken.has(to)) {
            problems.push(`${from} -> ${to}: that name is taken`);
        }
        taken.add(to);
    }
    return {renames, problems};
}
