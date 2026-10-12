// The programs running, as top shows them: oui_run processes.js, or oui_run_gui --window processes.js
// Every two seconds the table is read again from ps. A click on a heading orders the programs by it, again the
// other way; at first the busiest are on top. What is typed in the filter keeps only the programs whose name has
// it. Kill asks, then ends the program chosen. For Linux and the other systems with a ps that takes -o, as macOS's.
//
// The window is described in ui/processes.oui; the table goes in its Slot, which a description cannot fill.
const columns = ["pid", "cpu", "memory", "name"];
const ui = oui.uiFromFile("ui/processes.oui");
const table = ui.put("table", new oui.Table(["PID", "CPU %", "MEM %", "Program"], {widths: [9, 8, 8]}));

let processes = [];
// What the table shows, in its order, to know which program a row is.
let shown = [];
// The order asked for: busiest first until a heading says otherwise.
let order = {column: 1, descending: true};
table.sortBy(order.column, "descending");

async function read() {
    let text = "";
    const code = await oui.run("ps -A -o pid= -o pcpu= -o pmem= -o comm=", {output: (piece) => { text += piece; }});
    if (code !== 0) {
        ui.status.text = "ps could not be run.";
        return;
    }
    processes = text.split("\n")
        .map((line) => line.trim().match(/^(\d+)\s+([\d.]+)\s+([\d.]+)\s+(.+)$/))
        .filter((match) => match !== null)
        // macOS's ps gives a program's whole path where Linux's gives its name: the name is what shows.
        .map(([, pid, cpu, memory, command]) => ({pid: Number(pid), cpu: Number(cpu), memory: Number(memory),
                                                 name: command.slice(command.lastIndexOf("/") + 1)}));
    update();
}

function update() {
    const chosen = shown[table.selected]?.pid;
    const filter = ui.filter.text.toLowerCase();
    const key = columns[order.column];
    const compare = key === "name" ? (a, b) => a.name.localeCompare(b.name) : (a, b) => a[key] - b[key];
    shown = processes.filter((process) => process.name.toLowerCase().includes(filter))
        .sort((a, b) => (order.descending ? compare(b, a) : compare(a, b)) || a.pid - b.pid);
    table.rows = shown.map((process) => [process.pid, process.cpu.toFixed(1), process.memory.toFixed(1),
                                         process.name]);
    // The program chosen stays chosen as the table moves under it.
    const again = shown.findIndex((process) => process.pid === chosen);
    if (again >= 0) {
        table.selected = again;
    }
    ui.status.text = `${shown.length} of ${processes.length} programs.`;
}

table.onSort = (column, asked) => {
    // Back to the busiest first when a heading asks for no order.
    order = asked === "none" ? {column: 1, descending: true} : {column, descending: asked === "descending"};
    table.sortBy(order.column, order.descending ? "descending" : "ascending");
    update();
    // In another order, from its top.
    table.selected = 0;
};
ui.filter.onChange = update;
ui.kill.onClick = async () => {
    const process = shown[table.selected];
    if (process === undefined || !await oui.confirm(`End ${process.name}, ${process.pid}?`)) {
        return;
    }
    let said = "";
    const code = await oui.run(`kill ${process.pid}`, {output: (piece) => { said += piece; }});
    ui.status.text = code === 0 ? `Ended ${process.name}.` : `Could not end ${process.name}: ${said.trim()}`;
    await read();
};
ui.close.onClick = () => oui.quit();
oui.shortcut("Escape", () => oui.quit());
setInterval(read, 2000);
await read();
oui.window(ui);
ui.filter.focus();
