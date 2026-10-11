// Python.
{
    const keywords = [
        "and", "as", "assert", "async", "await", "break", "case", "class", "continue", "def", "del", "elif",
        "else", "except", "finally", "for", "from", "global", "if", "import", "in", "is", "lambda", "match",
        "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield",
    ];
    const types = [
        "bool", "bytearray", "bytes", "complex", "dict", "float", "frozenset", "int", "list", "object", "set",
        "str", "tuple", "type",
    ];
    const string = (quote) => ({
        kind: "string",
        rules: [{match: /\\./}, {match: new RegExp(quote), pop: true}, {match: /$/, pop: true}],
    });
    const triple = (quotes) => ({kind: "string", rules: [{match: /\\./}, {match: new RegExp(quotes), pop: true}]});
    oui.languages.register({
        id: "python",
        name: "Python",
        extensions: [".py", ".pyw", ".pyi"],
        comment: "#",
        // Its words, for the editor to suggest as one writes.
        suggestions: [
            ...keywords.map((word) => ({label: word, detail: "keyword"})),
            ...types.map((word) => ({label: word, detail: "type"})),
        ],
        states: {
            start: [
                {match: /#.*/, kind: "comment"},
                {match: /(?:\b[rRbBuUfF]{1,2})?"""/, push: "triple_double"},
                {match: /(?:\b[rRbBuUfF]{1,2})?'''/, push: "triple_single"},
                {match: /(?:\b[rRbBuUfF]{1,2})?"/, push: "double"},
                {match: /(?:\b[rRbBuUfF]{1,2})?'/, push: "single"},
                {match: /^\s*@[\w.]+/, kind: "preprocessor"},
                {match: /\b(?:0[xX][\da-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\d[\d_]*(?:\.[\d_]*)?(?:[eE][+-]?\d+)?)[jJ]?\b|(?<![\w.])\.\d+(?:[eE][+-]?\d+)?/, kind: "number"},
                {match: oui.words(keywords), kind: "keyword"},
                {match: oui.words("True", "False", "None", "self", "cls"), kind: "constant"},
                {match: oui.words(types), kind: "type"},
                {match: /(?<=\b(?:def|class)\s+)\w+/, kind: "function"},
                {match: /\b[A-Za-z_]\w*(?=\s*\()/, kind: "function"},
            ],
            double: string('"'),
            single: string("'"),
            triple_double: triple('"""'),
            triple_single: triple("'''"),
        },
    });
}
