// Commands that change text, in Tools > Text Commands. Each gets the selection, or the whole document when
// nothing is selected, and gives back what goes in its place.
{
    // The lines of `text`, and whether it ended with a line break, to put it back the same way.
    const lines = (text) => {
        const ended = text.endsWith("\n");
        return {list: (ended ? text.slice(0, -1) : text).split("\n"), ended};
    };
    const join = ({list, ended}) => list.join("\n") + (ended ? "\n" : "");
    const each_line = (change) => (text) => {
        const split = lines(text);
        split.list = change(split.list);
        return join(split);
    };
    const words = (text) => text.replace(/([a-z\d])([A-Z])/g, "$1 $2").split(/[\s_\-]+/).filter(Boolean);

    const commands = [
        {id: "text.sort_lines", title: {en: "Sort Lines", es: "Ordenar líneas"},
         run: each_line(list => [...list].sort((a, b) => a.localeCompare(b)))},
        {id: "text.unique_lines", title: {en: "Remove Duplicate Lines", es: "Quitar líneas repetidas"},
         run: each_line(list => [...new Set(list)])},
        {id: "text.reverse_lines", title: {en: "Reverse Lines", es: "Invertir el orden de las líneas"},
         run: each_line(list => [...list].reverse())},
        {id: "text.trim_trailing", title: {en: "Trim Trailing Whitespace", es: "Quitar espacios al final de línea"},
         run: each_line(list => list.map(line => line.replace(/\s+$/, "")))},
        {id: "text.upper_case", title: {en: "Upper Case", es: "Mayúsculas"}, run: text => text.toUpperCase()},
        {id: "text.lower_case", title: {en: "Lower Case", es: "Minúsculas"}, run: text => text.toLowerCase()},
        {id: "text.snake_case", title: {en: "snake_case", es: "snake_case"},
         run: text => text.replace(/[A-Za-z][\w\-]*/g, word => words(word).map(w => w.toLowerCase()).join("_"))},
        {id: "text.camel_case", title: {en: "camelCase", es: "camelCase"},
         run: text => text.replace(/[A-Za-z][\w\-]*/g, word => words(word)
             .map((w, i) => i === 0 ? w.toLowerCase() : w[0].toUpperCase() + w.slice(1).toLowerCase()).join(""))},
        {id: "text.format_json", title: {en: "Format JSON", es: "Formatear JSON"},
         run: text => JSON.stringify(JSON.parse(text), null, 4) + "\n"},
        {id: "text.minify_json", title: {en: "Minify JSON", es: "Compactar JSON"},
         run: text => JSON.stringify(JSON.parse(text))},
    ];
    for (const command of commands) {
        oui.commands.register(command);
    }
}
