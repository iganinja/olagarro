// rename.js in a window: oui_run rename_window.js [FOLDER], or oui_run_gui --window rename_window.js [FOLDER]
// As the pattern and the replacement are typed, the list shows what each file would be called; Rename does it.
//
// The window is described in ui/rename_window.oui, read with oui.uiFromFile: there each widget has an id, here it
// is ui.<id> and gets its callbacks. The list is not something a description writes, so it goes in the Slot.
import {join, quote} from "./lib/files.js";
import {planRenames} from "./lib/rename.js";

const folder = oui.args[0] ?? ".";
const ui = oui.uiFromFile("ui/rename_window.oui");
const preview = ui.put("preview", new oui.List());
let plan = {renames: [], problems: []};

function update() {
    let expression;
    try {
        expression = new RegExp(ui.pattern.text);
    } catch (error) {
        plan = {renames: [], problems: []};
        preview.items = [];
        ui.status.text = error.message;
        return;
    }
    plan = ui.pattern.text === "" ? {renames: [], problems: []}
                                  : planRenames(folder, expression, ui.replacement.text);
    preview.items = plan.problems.length > 0 ? plan.problems
                                             : plan.renames.map(({from, to}) => `${from} -> ${to}`);
    ui.status.text = plan.problems.length > 0 ? "Some new names are taken: nothing would be renamed."
                   : plan.renames.length === 0 ? "Nothing to rename."
                   : `${plan.renames.length} files to rename.`;
}

ui.pattern.onChange = update;
ui.replacement.onChange = update;
ui.rename.onClick = async () => {
    if (plan.problems.length > 0 || plan.renames.length === 0) {
        return;
    }
    const {renames} = plan;
    for (const {from, to} of renames) {
        if (await oui.run(`mv -- ${quote(join(folder, from))} ${quote(join(folder, to))}`) !== 0) {
            update();
            ui.status.text = `Could not rename ${from}; stopped there.`;
            return;
        }
    }
    update();
    ui.status.text = `Renamed ${renames.length} files.`;
};
ui.close.onClick = () => oui.quit();
oui.shortcut("Escape", () => oui.quit());
oui.window(ui);
ui.pattern.focus();
