// GDScript, Godot's language.
{
    const keywords = [
        "and", "as", "assert", "await", "break", "breakpoint", "class", "class_name", "const", "continue",
        "elif", "else", "enum", "extends", "for", "func", "if", "in", "is", "match", "not", "or", "pass",
        "preload", "return", "signal", "static", "super", "var", "void", "when", "while", "yield",
    ];
    const string = (quote) => ({
        kind: "string",
        rules: [{match: /\\./}, {match: new RegExp(quote), pop: true}, {match: /$/, pop: true}],
    });
    oui.languages.register({
        id: "gdscript",
        name: "GDScript",
        extensions: [".gd"],
        comment: "#",
        // Its words, for the editor to suggest as one writes.
        suggestions: keywords.map((word) => ({label: word, detail: "keyword"})),
        states: {
            start: [
                {match: /#.*/, kind: "comment"},
                {match: /"""/, push: "triple"},
                {match: /&?"/, push: "double"},
                {match: /&?'/, push: "single"},
                {match: /@\w+/, kind: "preprocessor"},
                {match: /\$(?:"[^"]*"|[\w\/]+)|%\w+/, kind: "constant"},
                {match: /\b(?:0[xX][\da-fA-F_]+|0[bB][01_]+|\d[\d_]*(?:\.[\d_]*)?(?:[eE][+-]?\d+)?)\b|(?<![\w.])\.\d+/, kind: "number"},
                {match: oui.words(keywords), kind: "keyword"},
                {match: oui.words("true", "false", "null", "self", "PI", "TAU", "INF", "NAN"), kind: "constant"},
                {match: oui.words("bool", "int", "float"), kind: "type"},
                {match: /\b[A-Z][A-Za-z0-9]*\b/, kind: "type"},
                {match: /(?<=\bfunc\s+)\w+/, kind: "function"},
                {match: /\b[a-z_]\w*(?=\s*\()/, kind: "function"},
            ],
            triple: {kind: "string", rules: [{match: /\\./}, {match: /"""/, pop: true}]},
            double: string('"'),
            single: string("'"),
        },
    });
}
