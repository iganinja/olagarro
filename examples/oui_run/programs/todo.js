// A list of things to do: oui_run todo.js [FILE], or oui_run_gui --window todo.js [FILE]
// What is typed goes on the list with Enter, or Add; a click on a box, Space or Done marks the one chosen done, and
// not done again; Delete takes it off. The list is kept in FILE, todo.json in the folder it starts in unless told,
// as JSON, after every change.
//
// The window is described in ui/todo.oui; the list goes in its Slot, which a description cannot fill.
const file = oui.args[0] ?? "todo.json";
const ui = oui.uiFromFile("ui/todo.oui");
const list = ui.put("list", new oui.List([], {checks: true}));
let items = JSON.parse(oui.readFile(file) ?? "[]");

function show(chosen = list.selected) {
    list.items = items.map((item) => item.text);
    list.checked = items.map((item) => item.done);
    list.selected = Math.min(chosen, items.length - 1);
    const left = items.filter((item) => !item.done).length;
    ui.status.text = items.length === 0 ? "Nothing to do." : `${left} of ${items.length} left to do.`;
}

function keep() {
    if (!oui.writeFile(file, JSON.stringify(items, null, 2) + "\n")) {
        ui.status.text = `Could not keep the list in ${file}.`;
    }
}

function add() {
    const text = ui.entry.text.trim();
    if (text === "") {
        return;
    }
    items.push({text, done: false});
    ui.entry.text = "";
    keep();
    show(items.length - 1);
}

function toggle() {
    const item = items[list.selected];
    if (item !== undefined) {
        item.done = !item.done;
        keep();
        show();
    }
}

ui.entry.onSubmit = add;
ui.add.onClick = add;
ui.done.onClick = toggle;
// The box ticked by the person, already showing so: only what is kept follows.
list.onToggle = (index, checked) => {
    items[index].done = checked;
    keep();
    show();
};
ui.remove.onClick = () => {
    if (list.selected >= 0 && list.selected < items.length) {
        items.splice(list.selected, 1);
        keep();
        show();
    }
};
ui.close.onClick = () => oui.quit();
oui.shortcut("Escape", () => oui.quit());
show(0);
oui.window(ui);
ui.entry.focus();
