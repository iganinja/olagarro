// What the tests of OUI's programs as a person uses them share: run by oui_run, each runs a program in a pseudo
// terminal by script(1), from a copy of it in a folder of its own, where it keeps its settings, so nothing of anyone's
// is touched; presses keys as it goes; and looks at the screen it leaves.

export const quote = (text) => `'${text.replaceAll("'", "'\\''")}'`;

// Runs a shell command and gives what it printed, its errors too, and its exit code.
export async function capture(command, options = {}) {
    let output = "";
    const code = await oui.run(command, {...options, output: (text) => { output += text; }});
    return {code, output};
}

// The command that runs `inner`, a shell command line, in a pseudo terminal by script(1): util-linux's on Linux; the
// BSD one on macOS, which takes the command after the file and has no -c.
const bsd = (await capture("uname -s")).output.trim() === "Darwin";
export const inPseudoTerminal = (inner) =>
    bsd ? `script -q /dev/null /bin/sh -c ${quote(inner)}` : `script -qec ${quote(inner)} /dev/null`;
// `command`, ended after `seconds`: by timeout on Linux; macOS has none, and perl's alarm ends it the same way.
export const endedAfter = (seconds, command) =>
    bsd ? `perl -e 'alarm shift; exec @ARGV' ${seconds} ${command}` : `timeout -s KILL ${seconds} ${command}`;

const pause = (milliseconds) => new Promise((done) => setTimeout(done, milliseconds));

// Until what is drawn has grown since `writes` and then stayed as it is for `quiet` milliseconds: the program has
// answered. At most `limit`, for a key that changes nothing on the screen.
async function answered(drawn, writes, {quiet = 200, limit = 3000} = {}) {
    const start = Date.now();
    while (Date.now() - start < limit) {
        await pause(25);
        if (drawn.writes > writes && Date.now() - drawn.last >= quiet) {
            return;
        }
    }
}

// A folder of its own with the program copied into it and the files given, which runs of the program share, as a
// person's starts of it do, its settings kept there from one to the next; remove() takes it away.
export async function place(program, files = {}) {
    const folder = (await capture("mktemp -d")).output.trim();
    const name = program.slice(program.lastIndexOf("/") + 1);
    await capture(`cp ${quote(program)} ${quote(folder)}/${quote(name)}`);
    for (const [path, text] of Object.entries(files)) {
        if (path.includes("/")) {
            await capture(`mkdir -p ${quote(`${folder}/${path.slice(0, path.lastIndexOf("/"))}`)}`);
        }
        oui.writeFile(`${folder}/${path}`, text);
    }
    return {
        folder,
        // Runs the program with the arguments, in a terminal of the size given, pressing each key once the program has
        // answered the one before, and gives the screen it left: the terminal is ended under it after the last key,
        // so that is what shows. A key is the text a terminal sends for it, "\r" for Enter; "" waits a moment.
        //
        // The keys go through a named pipe the terminal reads, held open so it never sees its input end; after each,
        // the program is waited for: until it has drawn something and then nothing for a moment, as a person waits
        // to see what a key did. A program slow to start or to answer, as on a machine with much else to do, is
        // waited for rather than pressed past.
        async run(args, pressed, {columns = 100, rows = 24} = {}) {
            const keys = `${folder}/.keys`;
            await capture(`rm -f ${quote(keys)}; mkfifo ${quote(keys)}`);
            const inner = `stty cols ${columns} rows ${rows}; ./${quote(name)} ${args.map(quote).join(" ")}`;
            const drawn = {text: "", writes: 0, last: Date.now()};
            const running = oui.run(
                // In a shell of its own whose errors go nowhere: the one that saw it stopped says so, over what it
                // drew. A sleep holds the pipe open to write, so it stays open however the keys are written to it: a
                // pipe opened to read and write at once is Linux's, which macOS does not open. cat hands the keys on
                // through a plain pipe: macOS's script stops at once when its input is a named pipe, which is a socket
                // there, and asked whether it is a terminal answers that a socket cannot be asked.
                `(sleep 86400 > ${quote(keys)} & exec 3< ${quote(keys)}; ` +
                    `cat <&3 | LANGUAGE=en LC_ALL=C LANG=C ${inPseudoTerminal(inner)}) 2>/dev/null`,
                {cwd: folder, output: (text) => {
                    drawn.text += text;
                    drawn.writes += 1;
                    drawn.last = Date.now();
                }});
            await answered(drawn, 0, {limit: 15000});
            for (const key of pressed) {
                if (key === "") {
                    await pause(600);
                    continue;
                }
                const before = drawn.writes;
                oui.writeFile(keys, key);
                await answered(drawn, before);
            }
            running.stop();
            await running;
            await capture(`rm -f ${quote(keys)}`);
            return screen(drawn.text, columns, rows);
        },
        remove: () => capture(`rm -rf ${quote(folder)}`),
    };
}

// The program run once, in a place of its own made for it and taken away after.
export async function session(program, args, files, pressed, size = {}) {
    const here = await place(program, files);
    const shown = await here.run(args, pressed, size);
    await here.remove();
    return shown;
}

// The screen a terminal 100 by 24 would show once everything written to it has been, from the cursor moves (CSI
// row;column H), the colours (CSI ... m, the sixteen of a console) and the text in between, which is all OUI writes;
// any other sequence is let pass. Each cell is its character and its colours as SGR numbers, "97;41" for white on
// red.
export function screen(written, width = 100, height = 24) {
    const cells = Array.from({length: height}, () => Array.from({length: width}, () => ({text: " ", colours: ""})));
    let row = 0;
    let column = 0;
    let colours = "";
    let index = 0;
    while (index < written.length) {
        if (written[index] === "\x1b") {
            const sequence = /^\x1b\[([?>\d;]*)([A-Za-z])/.exec(written.slice(index, index + 32));
            if (!sequence) {
                index += 2;
                continue;
            }
            const [whole, parameters, final] = sequence;
            if (final === "H") {
                const [r = 1, c = 1] = parameters.split(";").map(Number);
                row = r - 1;
                column = c - 1;
            } else if (final === "m") {
                colours = parameters.replace(/^0;?/, "");
            }
            index += whole.length;
            continue;
        }
        const character = String.fromCodePoint(written.codePointAt(index));
        index += character.length;
        if (character >= " " && row < height && column < width) {
            cells[row][column] = {text: character, colours};
            column += 1;
        }
    }
    return {
        // Everything written, as it came: to look for what leaves no trace on the screen, as a program ending.
        written,
        row: (number) => cells[number].map((cell) => cell.text).join(""),
        // Where a text first shows, as its row, column and the colours of its first character, or of the one
        // `offset` characters into it; none when nowhere.
        find(text, offset = 0) {
            for (let number = 0; number < height; number += 1) {
                const at = this.row(number).indexOf(text);
                if (at >= 0) {
                    return {row: number, column: at, colours: cells[number][at + offset].colours};
                }
            }
            return null;
        },
        all: () => cells.map((line) => line.map((cell) => cell.text).join("")).join("\n"),
    };
}

export function expect(what, actual, expected) {
    if (actual !== expected) {
        throw new Error(`${what}: ${JSON.stringify(actual)}, not ${JSON.stringify(expected)}`);
    }
}

// Runs the cases, or those whose name has `only` in it, says how each went, and ends with 1 when any failed.
// `only`, when given, chooses which: the cases whose name has it; or, as "part 2 of 4", every fourth from the second,
// so a long suite runs as parts side by side.
export async function run(cases, only) {
    let failed = 0;
    const part = /^part (\d+) of (\d+)$/.exec(only ?? "");
    const chosen = cases.filter((each, index) => !only ||
        (part ? index % Number(part[2]) === Number(part[1]) - 1 : each.name.includes(only)));
    for (const {name, run: work} of chosen) {
        try {
            await work();
            console.log(`ok   ${name}`);
        } catch (error) {
            failed += 1;
            console.log(`FAIL ${name}\n     ${error.message}`);
        }
    }
    console.log(failed === 0 ? `${chosen.length} passed` : `${failed} of ${chosen.length} failed`);
    oui.exit(failed === 0 ? 0 : 1);
}
