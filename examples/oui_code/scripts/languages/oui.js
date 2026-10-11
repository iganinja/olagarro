// OUI's user interface descriptions: a widget a line, its type, its text in quotes and its properties, name=value or
// a name alone.
oui.languages.register({
    id: "oui",
    name: "OUI Description",
    extensions: [".oui"],
    comment: "#",
    states: {
        start: [
            {match: /#.*/, kind: "comment"},
            {match: /"/, push: "string"},
            {match: /(?<==)\d+(?:\.\d+)?(?![^\s"])/, kind: "number"},
            {match: /(?<==)[^\s"]+/, kind: "constant"},
            {match: /\b[A-Z][A-Za-z0-9]*\b/, kind: "type"},
            {match: /\b[a-z_][\w-]*/, kind: "attribute"},
        ],
        string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}, {match: /$/, pop: true}]},
    },
});
