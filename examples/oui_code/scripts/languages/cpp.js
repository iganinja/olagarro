// C and C++.
{
    const keywords = [
        "alignas", "alignof", "asm", "auto", "break", "case", "catch", "class", "co_await", "co_return",
        "co_yield", "concept", "const", "const_cast", "consteval", "constexpr", "constinit", "continue",
        "decltype", "default", "delete", "do", "dynamic_cast", "else", "enum", "explicit", "export", "extern",
        "final", "for", "friend", "goto", "if", "import", "inline", "module", "mutable", "namespace", "new",
        "noexcept", "operator", "override", "private", "protected", "public", "register", "reinterpret_cast",
        "requires", "return", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "thread_local", "throw", "try", "typedef", "typeid", "typename", "union", "using",
        "virtual", "volatile", "while",
    ];
    const types = [
        "bool", "char", "char8_t", "char16_t", "char32_t", "double", "float", "int", "long", "short",
        "signed", "unsigned", "void", "wchar_t", "size_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t",
        "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t", "uintptr_t",
    ];
    oui.languages.register({
        id: "cpp",
        name: "C++",
        extensions: [".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".inl", ".ipp", ".cppm", ".ixx"],
        comment: "//",
        // Its words, for the editor to suggest as one writes.
        suggestions: [
            ...keywords.map((word) => ({label: word, detail: "keyword"})),
            ...types.map((word) => ({label: word, detail: "type"})),
        ],
        states: {
            start: [
                {match: /\/\/.*/, kind: "comment"},
                {match: /\/\*/, push: "block_comment"},
                {match: /^\s*#\s*\w+/, kind: "preprocessor", push: "directive"},
                {match: /\b(?:u8|u|U|L)?R"[^(\s"\\]{0,16}\(/, push: "raw_string"},
                {match: /(?:\b(?:u8|u|U|L))?"/, push: "string"},
                {match: /(?:\b(?:u8|u|U|L))?'(?:\\.|[^'\\])+'/, kind: "string"},
                {match: /\b(?:0[xX][\da-fA-F']+|0[bB][01']+|\d[\d']*(?:\.\d*)?(?:[eE][+-]?\d+)?)\w*|(?<![\w.])\.\d+(?:[eE][+-]?\d+)?\w*/, kind: "number"},
                {match: oui.words(keywords), kind: "keyword"},
                {match: oui.words(types), kind: "type"},
                {match: oui.words("true", "false", "nullptr", "NULL", "this"), kind: "constant"},
                {match: /\b[A-Za-z_]\w*(?=\s*\()/, kind: "function"},
            ],
            block_comment: {kind: "comment", rules: [{match: /\*\//, pop: true}]},
            string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}, {match: /$/, pop: true}]},
            raw_string: {kind: "string", rules: [{match: /\)[^"\s()\\]{0,16}"/, pop: true}]},
            // To the end of the line, unless a backslash carries it on.
            directive: {
                kind: "preprocessor",
                rules: [
                    {match: /\/\/.*/, kind: "comment"},
                    {match: /\/\*/, push: "block_comment"},
                    {match: /"/, push: "string"},
                    {match: /<[^>]*>/, kind: "string"},
                    {match: /(?<!\\)$/, pop: true},
                ],
            },
        },
    });
}
