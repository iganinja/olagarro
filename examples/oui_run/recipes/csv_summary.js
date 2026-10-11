// Groups the rows of a CSV file by one column and adds up another, as a pivot table does:
// oui_run csv_summary.js FILE GROUP_COLUMN [SUM_COLUMN]
// Columns go by their name in the first row. Without SUM_COLUMN it counts rows. Biggest first.
//
//     oui_run csv_summary.js sales.csv region amount
import {parseCsv} from "./lib/csv.js";
import {table} from "./lib/files.js";

const [file, groupColumn, sumColumn] = oui.args;
if (!file || !groupColumn) {
    console.log("usage: oui_run csv_summary.js FILE GROUP_COLUMN [SUM_COLUMN]");
    oui.exit(2);
}

const text = oui.readFile(file);
if (text === null) {
    console.log(`Cannot read ${file}`);
    oui.exit(1);
}
const [header, ...rows] = parseCsv(text);
const column = (name) => {
    const index = header.indexOf(name);
    if (index < 0) {
        console.log(`${file} has no column "${name}"; it has ${header.join(", ")}`);
        oui.exit(1);
    }
    return index;
};
const group = column(groupColumn);
const sum = sumColumn === undefined ? -1 : column(sumColumn);

const totals = new Map();
for (const row of rows) {
    const value = sum < 0 ? 1 : Number(row[sum]);
    if (Number.isNaN(value)) {
        console.log(`Not a number, left out: ${row.join(",")}`);
        continue;
    }
    totals.set(row[group], (totals.get(row[group]) ?? 0) + value);
}
// Biggest first; equal ones by name, so the order does not depend on the file's.
const sorted = [...totals].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));
// Two decimals at most, without the ones that are zero.
const rounded = (value) => Math.round(value * 100) / 100;
console.log(table([[groupColumn, sumColumn ?? "rows"], ...sorted.map(([name, total]) => [name, rounded(total)])]));
