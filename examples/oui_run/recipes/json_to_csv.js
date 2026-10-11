// A JSON array of objects as a CSV file, for a spreadsheet: oui_run json_to_csv.js FILE.json [OUT.csv]
// [--where=FIELD=VALUE] [--sort=FIELD]
// The columns are every field any object has, in the order they first appear; nested objects and arrays go in as
// JSON. Without OUT.csv the CSV goes to the terminal.
//
//     oui_run json_to_csv.js people.json people.csv --where=team=tools --sort=name
import {formatCsv} from "./lib/csv.js";
import {parseArgs} from "./lib/files.js";

const {options, positional} = parseArgs(oui.args);
const [input, output] = positional;
if (!input) {
    console.log("usage: oui_run json_to_csv.js FILE.json [OUT.csv] [--where=FIELD=VALUE] [--sort=FIELD]");
    oui.exit(2);
}

let records;
try {
    records = JSON.parse(oui.readFile(input) ?? "null");
} catch (error) {
    console.log(`${input} is not JSON: ${error.message}`);
    oui.exit(1);
}
if (!Array.isArray(records)) {
    console.log(`${input} is not an array`);
    oui.exit(1);
}

if (typeof options.where === "string") {
    const [field, value] = options.where.split("=");
    // Compared as text, so that --where=age=30 finds the number 30.
    records = records.filter((record) => String(record[field]) === value);
}
if (typeof options.sort === "string") {
    const field = options.sort;
    // localeCompare orders words as people do; numeric: true puts "item 9" before "item 10".
    records.sort((a, b) => String(a[field] ?? "").localeCompare(String(b[field] ?? ""), undefined, {numeric: true}));
}

const columns = [...new Set(records.flatMap((record) => Object.keys(record)))];
const cell = (value) => (value !== null && typeof value === "object" ? JSON.stringify(value) : value);
const csv = formatCsv([columns, ...records.map((record) => columns.map((column) => cell(record[column])))]);

if (output === undefined) {
    console.log(csv.trimEnd());
} else if (oui.writeFile(output, csv)) {
    console.log(`${output}: ${records.length} rows, ${columns.length} columns`);
} else {
    console.log(`Could not write ${output}`);
    oui.exit(1);
}
