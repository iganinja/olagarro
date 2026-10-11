// JavaScript and TypeScript.
{
    const keywords = [
        "async", "await", "break", "case", "catch", "class", "const", "continue", "debugger", "default",
        "delete", "do", "else", "export", "extends", "finally", "for", "from", "function", "get", "if",
        "import", "in", "instanceof", "let", "new", "of", "return", "set", "static", "super", "switch",
        "throw", "try", "typeof", "var", "void", "while", "with", "yield",
    ];
    const typescript = [
        "abstract", "as", "declare", "enum", "implements", "interface", "keyof", "namespace", "private",
        "protected", "public", "readonly", "satisfies", "type",
    ];
    const types = ["any", "bigint", "boolean", "never", "number", "object", "string", "symbol", "unknown"];
    const constants = ["true", "false", "null", "undefined", "NaN", "Infinity", "this"];
    // What the editor suggests as one writes: pieces of text for what is written often, with $0 where the cursor
    // goes after, and the words of the language.
    const suggestions = (extra) => [
        {label: "for", detail: "loop", text: "for (let i = 0; i < $0; i++) {\n}"},
        {label: "forof", detail: "loop over", text: "for (const $0 of ) {\n}"},
        {label: "if", detail: "if", text: "if ($0) {\n}"},
        {label: "function", detail: "function", text: "function $0() {\n}"},
        {label: "class", detail: "class", text: "class $0 {\n    constructor() {\n    }\n}"},
        {label: "try", detail: "try and catch", text: "try {\n    $0\n} catch (error) {\n}"},
        ...[...keywords, ...extra].map((word) => ({label: word, detail: "keyword"})),
        ...types.map((word) => ({label: word, detail: "type"})),
        ...constants.map((word) => ({label: word, detail: "constant"})),
    ];
    const states = (extra) => ({
        start: [
            {match: /\/\/.*/, kind: "comment"},
            {match: /\/\*/, push: "block_comment"},
            {match: /"/, push: "double_string"},
            {match: /'/, push: "single_string"},
            {match: /`/, push: "template"},
            // A regular expression literal, where an expression can start.
            {match: /(?<=(?:^|[=(,:;!&|?{}\[]|\breturn)\s*)\/(?![*\/])(?:\\.|\[(?:\\.|[^\]\\])*\]|[^\/\\\[])+\/[dgimsuvy]*/, kind: "string"},
            {match: /\b(?:0[xX][\da-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\d[\d_]*(?:\.[\d_]*)?(?:[eE][+-]?\d+)?)n?\b|(?<![\w.])\.\d+(?:[eE][+-]?\d+)?/, kind: "number"},
            {match: oui.words(keywords, extra), kind: "keyword"},
            {match: oui.words(constants), kind: "constant"},
            {match: oui.words(types), kind: "type"},
            {match: /[A-Za-z_$][\w$]*(?=\s*\()/, kind: "function"},
            // The end of a ${...} inside a template string.
            {match: /\}/, pop: true},
            {match: /\{/, push: "start"},
        ],
        block_comment: {kind: "comment", rules: [{match: /\*\//, pop: true}]},
        double_string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}, {match: /$/, pop: true}]},
        single_string: {kind: "string", rules: [{match: /\\./}, {match: /'/, pop: true}, {match: /$/, pop: true}]},
        template: {
            kind: "string",
            rules: [{match: /\\./}, {match: /`/, pop: true}, {match: /\$\{/, kind: "operator", push: "start"}],
        },
    });
    oui.languages.register({
        id: "javascript",
        name: "JavaScript",
        extensions: [".js", ".mjs", ".cjs", ".jsx"],
        comment: "//",
        states: states([]),
        suggestions: suggestions([]),
    });
    oui.languages.register({
        id: "typescript",
        name: "TypeScript",
        extensions: [".ts", ".mts", ".cts", ".tsx"],
        comment: "//",
        states: states(typescript),
        suggestions: suggestions(typescript),
    });
}
