// search.js with a window: a field for the pattern, the lines it is on in a list, and the lines around the one
// selected under it. oui_run search_window.js [FOLDER], or oui_run_gui --window search_window.js [FOLDER]
// Enter in the field searches; Up and Down in the list show each place; Escape or Ctrl+Q ends.
//
// A window is made of widgets, built as objects and laid out with oui.column, oui.row and oui.split; what they do
// is in their callbacks, onSubmit, onSelect and the rest, which change other widgets' properties.
import {readText, walk} from "./lib/files.js";

const folder = oui.args[0] ?? ".";
// How many lines to show over and under the one found.
const around = 6;

let places = [];

const status = new oui.Label(`Search in ${folder}: a regular expression, then Enter.`);
const results = new oui.List([], {onSelect: (index) => show(places[index])});
const preview = new oui.TextEdit({readOnly: true});
const field = new oui.LineEdit({placeholder: "Regular expression", onSubmit: search});

function search(pattern) {
    let expression;
    try {
        expression = new RegExp(pattern, "i");
    } catch (error) {
        status.text = error.message;
        return;
    }
    places = [];
    for (const file of walk(folder)) {
        const lines = readText(file)?.split("\n") ?? [];
        lines.forEach((line, index) => {
            if (expression.test(line)) {
                places.push({file, line: index, lines});
            }
        });
    }
    results.items = places.map((place) => `${place.file}:${place.line + 1}: ${place.lines[place.line].trim()}`);
    status.text = places.length === 0 ? "No matches." : `${places.length} matches. Up and Down go through them.`;
    if (places.length > 0) {
        results.selected = 0;
        show(places[0]);
        results.focus();
    } else {
        preview.text = "";
    }
}

// The lines around a place, numbered, the one found marked.
function show(place) {
    if (!place) {
        return;
    }
    const first = Math.max(0, place.line - around);
    const last = Math.min(place.lines.length, place.line + around + 1);
    const width = String(last).length;
    preview.text = place.lines.slice(first, last).map((text, offset) => {
        const number = first + offset;
        return `${number === place.line ? ">" : " "} ${String(number + 1).padStart(width)}  ${text}`;
    }).join("\n");
}

oui.shortcut("Escape", () => oui.quit());
oui.window(`Search ${folder}`, oui.column([
    field,
    status,
    oui.fill(oui.split(results, preview, {vertical: true, ratio: 0.5})),
]));
field.focus();
