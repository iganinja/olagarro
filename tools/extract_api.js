// Makes oui/oui.d.ts and doc/oui_api.md, what OUI's scripts can use besides JavaScript itself, from the comments
// marked "api" beside the code that defines each name:
//
//     // api(<where>): <declaration, as TypeScript writes it>
//     // What it does, on as many lines as it takes.
//
// <where> says which scripts have it: everywhere, system (oui_run and OUI Code's Run Script and Interactive Script),
// oui_run, oui_code (OUI Code's language and command scripts), or colouring (oui_code's and oui_run's, the rules that
// colour text being the same in both). A declaration is a function, `oui.readFile(path:
// string): string | null`; a value, `oui.args: string[]`; a class, `class oui.Label extends oui.Widget`, with its
// constructor, `new oui.Label(text?: string)`, and its members, `oui.Label#text: string`.
//
// Run with oui_run from the repository: `oui_run tools/extract_api.js` writes the files; with --check it writes
// nothing and fails when a file is not what the comments make, or when oui_run's own `oui` has a name the
// comments do not, or the other way round.

const sources = ["oui", "examples"];
// What it makes, and from what.
const outputs = [
    {path: "oui/oui.d.ts", make: (found) => declarations(found)},
    {path: "doc/oui_api.md", make: (found) => reference(found)},
];

const where = {
    everywhere: "Every script.",
    system: "oui_run, and OUI Code's Tools > Run Script and Tools > Interactive Script.",
    oui_run: "oui_run.",
    oui_code: "OUI Code's language and command scripts.",
    colouring: "OUI Code's language and command scripts, and oui_run.",
};

// Every source file under the folders, in a fixed order.
function files(root, folder, found = []) {
    for (const name of oui.listDir(`${root}/${folder}`)) {
        const path = `${folder}/${name.replace(/\/$/, "")}`;
        if (name.endsWith("/")) {
            files(root, path, found);
        } else if (/\.(cpp|h|js)$/.test(name)) {
            found.push(path);
        }
    }
    return found;
}

// Each comment marked "api", with the lines of comment under it.
export function entries(root) {
    const found = [];
    for (const file of sources.flatMap((folder) => files(root, folder))) {
        let current = null;
        for (const raw of (oui.readFile(`${root}/${file}`) ?? "").split("\n")) {
            const line = raw.trim();
            const marked = /^\/\/ api\((\w+)\): (.+)$/.exec(line);
            if (marked) {
                if (!(marked[1] in where)) {
                    throw new Error(`${file}: no scripts are called "${marked[1]}"`);
                }
                current = {where: marked[1], declaration: marked[2].trim(), doc: [], file};
                found.push(current);
            } else if (current && line.startsWith("// ") && line.length > 3) {
                current.doc.push(line.slice(3));
            } else {
                current = null;
            }
        }
    }
    return found;
}

// What a declaration declares: its kind, the whole name of what it is in or is, and the rest as TypeScript writes it.
function parse(declaration) {
    let match;
    if ((match = /^class ([\w.]+)(?: extends ([\w.]+))?$/.exec(declaration))) {
        return {kind: "class", name: match[1], base: match[2]};
    }
    if ((match = /^new ([\w.]+)(\(.*)$/.exec(declaration))) {
        return {kind: "constructor", name: match[1], rest: `constructor${match[2]}`};
    }
    if ((match = /^([\w.]+)#(\w+)(.*)$/.exec(declaration))) {
        return {kind: "member", name: match[1], rest: `${match[2]}${match[3]}`};
    }
    if ((match = /^([\w.]+)(\(.*)$/.exec(declaration))) {
        return {kind: "function", name: match[1], rest: match[2]};
    }
    if ((match = /^([\w.]+)(: .*)$/.exec(declaration))) {
        return {kind: "value", name: match[1], rest: match[2]};
    }
    throw new Error(`cannot read the declaration "${declaration}"`);
}

const last = (name) => name.slice(name.lastIndexOf(".") + 1);
const container = (name) => (name.includes(".") ? name.slice(0, name.lastIndexOf(".")) : "");

function comment(entry, indent) {
    const lines = [...entry.doc, `In: ${where[entry.where]}`];
    return [`${indent}/**`, ...lines.map((line) => `${indent} * ${line}`), `${indent} */`];
}

const byName = (a, b) => (a < b ? -1 : a > b ? 1 : 0);

// The entries as what they declare: each namespace, "" for the global one, with its values, functions and classes,
// and each class with its constructor and members.
function organise(found) {
    const classes = new Map();
    const namespaces = new Map([["", {values: [], functions: [], classes: []}]]);
    const namespace = (name) => {
        if (!namespaces.has(name)) {
            namespaces.set(name, {values: [], functions: [], classes: []});
            namespace(container(name));
        }
        return namespaces.get(name);
    };
    for (const entry of found) {
        const declared = {...entry, ...parse(entry.declaration)};
        if (declared.kind === "class") {
            classes.set(declared.name, {...declared, members: []});
            namespace(container(declared.name)).classes.push(declared.name);
        } else if (declared.kind === "constructor" || declared.kind === "member") {
            if (!classes.has(declared.name)) {
                throw new Error(`${entry.file}: ${declared.name} is not declared as a class before its members`);
            }
            classes.get(declared.name).members.push(declared);
        } else {
            namespace(container(declared.name))[declared.kind === "value" ? "values" : "functions"].push(declared);
        }
    }
    return {classes, namespaces};
}

// The namespaces directly inside name, in order.
const inside = (namespaces, name) =>
    [...namespaces.keys()].filter((other) => other !== name && container(other) === name).sort(byName);

// The declarations file, from the entries.
export function declarations(found) {
    const {classes, namespaces} = organise(found);
    const lines = [
        "// What OUI's scripts can use, besides JavaScript itself. tools/extract_api.js makes it from the comments marked",
        "// \"api\" beside the code that defines each name: change those, and run it again, rather than this file.",
    ];
    const write = (name, indent) => {
        const space = namespaces.get(name);
        const top = name === "";
        // A blank line between declarations, none after the brace that opens the namespace.
        let first = !top;
        const gap = () => {
            if (!first) {
                lines.push("");
            }
            first = false;
        };
        for (const value of [...space.values].sort((a, b) => byName(a.name, b.name))) {
            gap();
            lines.push(...comment(value, indent), `${indent}${top ? "declare " : ""}const ${last(value.name)}${value.rest};`);
        }
        for (const fn of [...space.functions].sort((a, b) => byName(a.name, b.name))) {
            gap();
            lines.push(...comment(fn, indent), `${indent}${top ? "declare " : ""}function ${last(fn.name)}${fn.rest};`);
        }
        for (const className of [...space.classes].sort(byName)) {
            const declared = classes.get(className);
            const base = declared.base ? ` extends ${container(declared.base) === name ? last(declared.base) : declared.base}` : "";
            gap();
            lines.push(...comment(declared, indent), `${indent}${top ? "declare " : ""}class ${last(className)}${base} {`);
            declared.members.forEach((member, index) => {
                if (index > 0 || member.doc.length > 0) {
                    lines.push(...(index > 0 ? [""] : []), ...(member.doc.length ? comment(member, indent + "    ") : []));
                }
                lines.push(`${indent}    ${member.rest};`);
            });
            lines.push(`${indent}}`);
        }
        for (const other of inside(namespaces, name)) {
            gap();
            lines.push(`${indent}${top ? "declare " : ""}namespace ${last(other)} {`);
            write(other, indent + "    ");
            lines.push(`${indent}}`);
        }
    };
    write("", "");
    return lines.join("\n") + "\n";
}


// The anchor GitHub gives a heading: lower case, without what is neither a letter, a digit, a space nor a hyphen,
// spaces as hyphens.
const anchor = (heading) => heading.toLowerCase().replace(/[^a-z0-9 _-]/g, "").replace(/ /g, "-");

// The reference in Markdown, from the entries: every name, under its namespace, in the order of the declarations.
export function reference(found) {
    const {classes, namespaces} = organise(found);
    const order = [];
    const visit = (name) => {
        order.push(name);
        for (const other of inside(namespaces, name)) {
            visit(other);
        }
    };
    visit("");
    const title = (name) => (name === "" ? "Globals" : name);
    const sorted = (list) => [...list].sort((a, b) => byName(a.name, b.name));
    const names = (name) => {
        const space = namespaces.get(name);
        return [...sorted(space.values), ...sorted(space.functions)].map((entry) => entry.name)
            .concat([...space.classes].sort(byName));
    };
    // Each heading's anchor, in the order they come: GitHub numbers a repeated one, "-1" on the second, as oui.ui
    // and the class oui.Ui are, or the forms of one function. A name links to its first.
    const anchors = new Map();
    const seen = new Map();
    const heading = (text) => {
        const plain = anchor(text);
        const count = seen.get(plain) ?? 0;
        seen.set(plain, count + 1);
        if (!anchors.has(text)) {
            anchors.set(text, count === 0 ? plain : `${plain}-${count}`);
        }
    };
    heading("OUI script API");
    heading("Contents");
    for (const name of order) {
        const space = namespaces.get(name);
        if (names(name).length === 0) {
            continue;
        }
        heading(title(name));
        [...sorted(space.values), ...sorted(space.functions)].forEach((entry) => heading(entry.name));
        [...space.classes].sort(byName).forEach(heading);
    }
    const link = (text) => anchors.get(text) ?? anchor(text);
    const lines = [
        "# OUI script API",
        "",
        "*tools/extract_api.js makes this file from the comments marked \"api\" beside the code that defines each name:",
        "change those, and run it again, rather than this file.*",
        "",
        "Everything OUI's scripts can use besides JavaScript itself, name by name. [Scripting](scripting.md) shows",
        "how to put it to work; `oui/oui.d.ts` has the same as TypeScript declarations, for editors that read them.",
        "",
        "Not every script has every name: each entry ends saying which have it.",
        "",
        "## Contents",
        "",
    ];
    for (const name of order) {
        const listed = names(name);
        if (listed.length > 0) {
            lines.push(`- [${title(name)}](#${link(title(name))}): ` +
                listed.map((entry) => `[${last(entry)}](#${link(entry)})`).join(", "));
        }
    }
    const describe = (entry) => [...(entry.doc.length ? [entry.doc.join(" "), ""] : []), `*In: ${where[entry.where]}*`];
    for (const name of order) {
        const space = namespaces.get(name);
        if (names(name).length === 0) {
            continue;
        }
        lines.push("", `## ${title(name)}`);
        for (const entry of [...sorted(space.values), ...sorted(space.functions)]) {
            lines.push("", `### ${entry.name}`, "", "```ts", entry.declaration, "```", "", ...describe(entry));
        }
        for (const className of [...space.classes].sort(byName)) {
            const declared = classes.get(className);
            const made = declared.members.find((member) => member.kind === "constructor");
            lines.push("", `### ${className}`, "", "```ts", declared.declaration);
            if (made) {
                lines.push(made.declaration);
            }
            lines.push("```", "", ...describe(declared));
            const members = declared.members.filter((member) => member.kind === "member");
            if (declared.base) {
                lines.push("", `Has what [${declared.base}](#${link(declared.base)}) has, and:`);
            }
            if (members.length > 0) {
                lines.push("");
                for (const member of members) {
                    lines.push(`- \`${member.rest}\`${member.doc.length ? `: ${member.doc.join(" ")}` : ""}`);
                }
            }
        }
    }
    return lines.join("\n") + "\n";
}

// The names oui_run's own `oui` has and the comments do not, and those they declare that it has not.
function unlike(found) {
    const declared = new Set(
        found
            .filter((entry) => ["system", "oui_run", "colouring"].includes(entry.where))
            .map((entry) => parse(entry.declaration))
            .filter((parsed) => parsed.kind !== "constructor" && parsed.kind !== "member")
            .map((parsed) => parsed.name)
            .filter((name) => name.startsWith("oui.") && name.split(".").length === 2));
    const live = new Set(Object.keys(oui).map((name) => `oui.${name}`));
    // oui.Widget is not made by scripts, only what derives from it.
    live.add("oui.Widget");
    return [
        ...[...live].filter((name) => !declared.has(name)).map((name) => `${name} has no comment marked "api"`),
        ...[...declared].filter((name) => !live.has(name)).map((name) => `${name} is declared, but oui has no such name`),
    ];
}

if (oui.args !== undefined) {
    const check = oui.args.includes("--check");
    const root = oui.args.find((argument) => !argument.startsWith("--")) ?? ".";
    const found = entries(root);
    const problems = check ? unlike(found) : [];
    for (const {path, make} of outputs) {
        const made = make(found);
        if (check) {
            if (oui.readFile(`${root}/${path}`) !== made) {
                problems.push(`${path} is not what the comments make: run oui_run tools/extract_api.js`);
            }
        } else if (oui.writeFile(`${root}/${path}`, made)) {
            console.log(`${path}: ${found.length} declarations`);
        } else {
            problems.push(`Could not write ${path}`);
        }
    }
    for (const problem of problems) {
        console.log(problem);
    }
    oui.exit(problems.length === 0 ? 0 : 1);
}
