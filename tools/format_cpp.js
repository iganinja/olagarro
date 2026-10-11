// Formats the repository's C++: clang-format with the repository's .clang-format, then the space the conventions want
// after a prefix increment or decrement (`++ index`), which clang-format takes away.
//
// Run with oui_run from the repository: `oui_run tools/format_cpp.js` formats every C++ source but those of
// third_party and of the build folders; `oui_run tools/format_cpp.js FILE...` only those. --clang-format=PATH names
// the program, when it is not clang-format on the PATH.

const extensions = [".h", ".hpp", ".cpp", ".cc", ".cxx"];
// third_party holds other projects' code, kept as upstream wrote it.
const skipped = (name) => name === ".git" || name === "third_party" || name === "dist" || name.startsWith("bin") ||
    name.startsWith("build");
// How many files go to each run of clang-format, so its command line stays within what every system takes.
const batch = 40;

// Every C++ source under the folder, in a fixed order.
function sources(folder, found = []) {
    for (const name of oui.listDir(folder)) {
        const path = `${folder}/${name.replace(/\/$/, "")}`;
        if (name.endsWith("/")) {
            if (!skipped(name.slice(0, -1))) {
                sources(path, found);
            }
        } else if (extensions.some((extension) => name.endsWith(extension))) {
            found.push(path);
        }
    }
    return found;
}

// A raw string literal's opening, R"delimiter( with an optional encoding prefix, whose text is taken as it is:
// quotes and backslashes inside mean nothing until )delimiter".
const rawString = /(?:u8|u|U|L)?R"([^()\\ \t\n]{0,16})\(/y;
const isWordCharacter = (character) => /[A-Za-z0-9_]/.test(character);

// `text` with a space after each prefix ++ and -- of the code, leaving comments, strings and characters alone.
export function spacePrefixOperators(text) {
    let result = "";
    let state = "code";
    let index = 0;
    while (index < text.length) {
        const character = text[index];
        const following = text[index + 1] ?? "";
        if (state === "code") {
            rawString.lastIndex = index;
            const raw = rawString.exec(text);
            if (raw && (index === 0 || !isWordCharacter(text[index - 1]))) {
                const close = text.indexOf(`)${raw[1]}"`, rawString.lastIndex);
                const end = close < 0 ? text.length : close + raw[1].length + 2;
                result += text.slice(index, end);
                index = end;
                continue;
            }
            if (character === "/" && (following === "/" || following === "*")) {
                result += character + following;
                index += 2;
                state = following === "/" ? "line comment" : "block comment";
                continue;
            }
            if (character === '"') {
                state = "string";
            } else if (character === "'") {
                state = "character";
            } else if ((character === "+" || character === "-") && following === character &&
                       /[A-Za-z_]/.test(text[index + 2] ?? "")) {
                result += character + following + " ";
                index += 2;
                continue;
            }
        } else if (state === "line comment") {
            if (character === "\n") {
                state = "code";
            }
        } else if (state === "block comment") {
            if (character === "*" && following === "/") {
                result += "*/";
                index += 2;
                state = "code";
                continue;
            }
        } else {
            if (character === "\\") {
                result += text.slice(index, index + 2);
                index += 2;
                continue;
            }
            if ((state === "string" && character === '"') || (state === "character" && character === "'")) {
                state = "code";
            }
        }
        result += character;
        index += 1;
    }
    return result;
}

if (oui.args !== undefined) {
    const clangFormat = oui.args.find((argument) => argument.startsWith("--clang-format="))?.slice(15) ??
        "clang-format";
    const given = oui.args.filter((argument) => !argument.startsWith("--"));
    const files = given.length > 0 ? given : sources(".");
    for (let start = 0; start < files.length; start += batch) {
        const quoted = files.slice(start, start + batch).map((file) => `"${file}"`).join(" ");
        const code = await oui.run(`"${clangFormat}" -i ${quoted}`);
        if (code !== 0) {
            console.log(`${clangFormat} ended with code ${code}`);
            oui.exit(1);
        }
    }
    for (const file of files) {
        const text = oui.readFile(file);
        const formatted = text === null ? null : spacePrefixOperators(text);
        if (formatted !== null && formatted !== text && !oui.writeFile(file, formatted)) {
            console.log(`Could not write ${file}`);
            oui.exit(1);
        }
    }
}
