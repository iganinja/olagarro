// Reading and writing CSV, as spreadsheets do.

// The rows of CSV text, each an array of fields, as spreadsheets write it: commas between fields, double quotes
// around a field with a comma, a quote or a line break in it, and a quote inside one written twice.
export function parseCsv(text) {
    const rows = [];
    let row = [];
    let field = "";
    let quoted = false;
    for (let index = 0; index < text.length; index += 1) {
        const character = text[index];
        if (quoted) {
            if (character === '"' && text[index + 1] === '"') {
                field += '"';
                index += 1;
            } else if (character === '"') {
                quoted = false;
            } else {
                field += character;
            }
        } else if (character === '"') {
            quoted = true;
        } else if (character === ",") {
            row.push(field);
            field = "";
        } else if (character === "\n" || character === "\r") {
            // \r\n is one line break.
            if (character === "\r" && text[index + 1] === "\n") {
                index += 1;
            }
            row.push(field);
            rows.push(row);
            row = [];
            field = "";
        } else {
            field += character;
        }
    }
    if (field !== "" || row.length > 0) {
        row.push(field);
        rows.push(row);
    }
    return rows;
}

// A field as CSV writes it: in double quotes, with any inside written twice, when it has a comma, a quote or a line
// break; as it is otherwise.
export function csvField(value) {
    const text = String(value ?? "");
    return /[",\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

// Rows, each an array of fields, as CSV text, a line each.
export function formatCsv(rows) {
    return rows.map((row) => row.map(csvField).join(",")).join("\n") + "\n";
}
