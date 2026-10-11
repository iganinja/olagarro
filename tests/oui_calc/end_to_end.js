// OUI Calc as a person uses it, in a terminal: run by oui_run, tests/oui_calc/end_to_end.js PATH_TO_OUI_CALC [CASE],
// on what tests/lib/terminal_session.js does: run in a pseudo terminal, keys pressed, the screen it leaves looked at.
import {expect, run, session} from "../lib/terminal_session.js";

const [ouiCalc, only] = oui.args;
if (!ouiCalc) {
    console.log("usage: oui_run tests/oui_calc/end_to_end.js PATH_TO_OUI_CALC [CASE]");
    oui.exit(2);
}

const keys = {enter: "\r", up: "\x1b[A", f1: "\x1bOP", f2: "\x1bOQ"};
const typed = (...lines) => lines.map((line) => line + keys.enter);
const calculate = (pressed) => session(ouiCalc, [], {}, pressed, {columns: 80, rows: 20});

const cases = [];
const test = (name, work) => cases.push({name, run: work});

test("Each expression goes to the log with what it came to, and a name kept is used after", async () => {
    const shown = await calculate(typed("(800*0.16) - (0.32 * 500)", "interes = 800*0.03", "interes + 800"));
    expect("the first", shown.find("= -32") !== null, true);
    expect("the name kept", shown.find("interes = 24") !== null, true);
    expect("the name used", shown.find("= 824") !== null, true);
    // What it came to in white, what was written in grey: Turbo's keyword and comment on blue.
    expect("a result's colours", shown.find("= 824")?.colours, "97;44");
    expect("what was written", shown.find("> interes + 800")?.colours, "37;44");
});

test("What the line comes to shows as it is written, before Enter", async () => {
    const shown = await calculate(["800 + 16%"]);
    expect("the share added", shown.find("= 928") !== null, true);
    expect("only on the line, not in the log", shown.find("> 800")?.row, 19);
});

test("A mistake stays on the line, and says what is wrong", async () => {
    const shown = await calculate(typed("2 * banana"));
    expect("why", shown.find("Unknown name banana")?.colours.startsWith("91"), true);
    expect("the line as it was", shown.row(19).startsWith("> 2 * banana"), true);
    expect("nothing in the log", shown.find("> 2 * banana")?.row, 19);
});

test("Up brings back what was written before", async () => {
    const shown = await calculate([...typed("6 * 7"), keys.up]);
    expect("the line", shown.row(19).startsWith("> 6 * 7"), true);
    expect("what it comes to", shown.row(18).startsWith("= 42"), true);
});

test("deg measures angles in degrees", async () => {
    const shown = await calculate(typed("deg", "sin(30)"));
    expect("the sine", shown.find("= 0.5") !== null, true);
    expect("what the angles are", shown.find("DEG") !== null, true);
});

test("A click on the angles' unit switches between radians and degrees", async () => {
    // Where RAD shows first: the end of the line under the log.
    const before = await calculate([]);
    const unit = before.find("RAD");
    expect("the unit", unit !== null, true);
    // A press and a release there, as a terminal reports them, from 1.
    const click = (row, column) => [`\x1b[<0;${column + 1};${row + 1}M`, `\x1b[<0;${column + 1};${row + 1}m`];
    const shown = await calculate([...click(unit.row, unit.column + 1), ...typed("sin(30)")]);
    expect("degrees", shown.find("DEG") !== null, true);
    expect("the sine in degrees", shown.find("= 0.5") !== null, true);
});

test("F2 opens the options, a box to tick for degrees, all from the keyboard", async () => {
    const options = await calculate([keys.f2]);
    expect("the box, not ticked in radians", options.find("[ ] Measure angles in degrees") !== null, true);
    // Space ticks it, Enter keeps it.
    const shown = await calculate([keys.f2, " ", keys.enter, ...typed("sin(30)")]);
    expect("degrees", shown.find("DEG") !== null, true);
    expect("the sine in degrees", shown.find("= 0.5") !== null, true);
});

test("exit ends it, and is no name", async () => {
    const named = await calculate(typed("exit = 1"));
    expect("why", named.find("exit is a word of the calculator's own") !== null, true);
    const shown = await calculate(typed("exit"));
    // A program that ends gives the terminal back its own screen; one killed after the keys does not.
    expect("it ended", shown.written.includes("\x1b[?1049l"), true);
});

test("F1 explains it all", async () => {
    const shown = await calculate([keys.f1]);
    expect("the help", shown.find("Calculator Help") !== null, true);
    expect("its first part", shown.find("Write an expression as you would on paper") !== null, true);
    // Further down, the program's own key, after the calculator's.
    const further = await calculate([keys.f1, "\x1b[F"]);
    expect("how to quit", further.find("Ctrl+Q") !== null, true);
});

await run(cases, only);
