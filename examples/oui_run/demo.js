// A small panel built with OUI from JavaScript: oui_run demo.js (or oui_run_gui --window demo.js)
// A list of actions on the left, what the selected one is for over the output of what it runs.
import {actions} from "./demo_actions.js";

const description = new oui.Label("", {wrap: true});
const output = new oui.TextEdit({readOnly: true});
let running = null;

async function start(action) {
    if (running) {
        await oui.message("Busy", "Something is running already; Esc stops it.");
        return;
    }
    if (action.confirm && !(await oui.confirm(action.confirm))) {
        return;
    }
    output.append(`$ ${action.command}\n`);
    running = oui.run(action.command, {output: (text) => output.append(text)});
    const code = await running;
    running = null;
    output.append(`[exit ${code}]\n\n`);
}

const list = new oui.List(actions.map((action) => action.title), {
    onSelect: (row) => { description.text = actions[row].description; },
    onActivate: (row) => start(actions[row]),
});
description.text = actions[0].description;

oui.shortcut("Escape", () => running?.stop());
oui.window("Demo panel", oui.split(
    list,
    oui.column([description, oui.fill(output)]),
    {ratio: 0.3},
));
list.focus();
