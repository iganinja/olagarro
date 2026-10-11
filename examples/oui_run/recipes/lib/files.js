// What several recipes need, as a module they import: going through a folder, telling text from binary files,
// reading the command line and quoting for the shell.

// Folders nobody means to search: version control's, and what builds and package managers make.
export const skipped = new Set([".git", ".hg", ".svn", "node_modules", "build"]);

// A name inside folder, without a leading "./" when folder is the current one.
export function join(folder, name) {
    return folder === "." ? name : `${folder.replace(/\/$/, "")}/${name}`;
}

// Every file under folder, its subfolders' too, in order, as paths that start with folder. listDir ends a folder's
// name in "/", which is how this tells them from files.
export function walk(folder, found = []) {
    for (const name of oui.listDir(folder)) {
        if (!name.endsWith("/")) {
            found.push(join(folder, name));
        } else if (!skipped.has(name.slice(0, -1))) {
            walk(join(folder, name.slice(0, -1)), found);
        }
    }
    return found;
}

// A test on a path, from patterns such as "*.js" or "*.cpp,*.h": "*" is any run of characters, "?" any one. Only
// the file's name is looked at. No patterns match everything.
export function matcher(patterns) {
    const list = (patterns ?? "").split(",").map((pattern) => pattern.trim()).filter(Boolean);
    if (list.length === 0) {
        return () => true;
    }
    const expressions = list.map((pattern) => new RegExp(
        `^${pattern.replace(/[.+^${}()|[\]\\]/g, "\\$&").replaceAll("*", ".*").replaceAll("?", ".")}$`));
    return (path) => {
        const name = path.slice(path.lastIndexOf("/") + 1);
        return expressions.some((expression) => expression.test(name));
    };
}

// The file's text; null when it cannot be read, or when it is not text: a NUL character gives a binary file away.
export function readText(path) {
    const text = oui.readFile(path);
    return text === null || text.includes("\0") ? null : text;
}

// The command line: "--name" and "--name=value" as options, the rest as positional arguments, in order.
export function parseArgs(args) {
    const options = {};
    const positional = [];
    for (const argument of args) {
        const option = /^--([\w-]+)(?:=(.*))?$/.exec(argument);
        if (option) {
            options[option[1]] = option[2] ?? true;
        } else {
            positional.push(argument);
        }
    }
    return {options, positional};
}

// text as one argument for a POSIX shell, whatever is in it: oui.run(`mv ${quote(from)} ${quote(to)}`).
export function quote(text) {
    return `'${text.replaceAll("'", "'\\''")}'`;
}

// Text in columns, each as wide as its widest cell; a column with numbers in it aligned to the right, its heading
// too.
export function table(rows) {
    const widths = [];
    const numeric = [];
    for (const row of rows) {
        row.forEach((cell, index) => {
            widths[index] = Math.max(widths[index] ?? 0, String(cell).length);
            numeric[index] = numeric[index] || typeof cell === "number";
        });
    }
    return rows.map((row) => row.map((cell, index) => (numeric[index]
        ? String(cell).padStart(widths[index])
        : String(cell).padEnd(widths[index]))).join("  ").trimEnd()).join("\n");
}
