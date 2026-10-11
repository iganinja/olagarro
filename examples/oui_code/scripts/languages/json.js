// JSON, with the comments some JSON files allow.
oui.languages.register({
    id: "json",
    name: "JSON",
    extensions: [".json", ".jsonc", ".json5"],
    comment: "//",
    states: {
        start: [
            {match: /\/\/.*/, kind: "comment"},
            {match: /\/\*/, push: "block_comment"},
            {match: /"(?:\\.|[^"\\])*"(?=\s*:)/, kind: "attribute"},
            {match: /"/, push: "string"},
            {match: /-?\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b/, kind: "number"},
            {match: oui.words("true", "false", "null"), kind: "constant"},
        ],
        block_comment: {kind: "comment", rules: [{match: /\*\//, pop: true}]},
        string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}, {match: /$/, pop: true}]},
    },
});
