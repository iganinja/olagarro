// Markdown.
oui.languages.register({
    id: "markdown",
    name: "Markdown",
    extensions: [".md", ".markdown"],
    brackets: ["()", "[]"],
    states: {
        start: [
            {match: /^\s*(?:```|~~~).*/, kind: "string", push: "fence"},
            {match: /^#{1,6}\s.*/, kind: "heading"},
            {match: /^\s*>.*/, kind: "comment"},
            {match: /<!--/, push: "html_comment"},
            {match: /^\s*(?:[-*+]|\d+[.)])(?=\s)/, kind: "keyword"},
            {match: /^\s*(?:-{3,}|\*{3,}|_{3,})\s*$/, kind: "keyword"},
            {match: /`[^`]+`/, kind: "string"},
            {match: /!?\[[^\]]*\]\([^)]*\)/, kind: "link"},
            {match: /\*\*[^*]+\*\*|__[^_]+__/, kind: "emphasis"},
            {match: /(?<![\w*])\*[^*\s][^*]*\*(?!\w)|(?<!\w)_[^_\s][^_]*_(?!\w)/, kind: "emphasis"},
            {match: /\|/, kind: "operator"},
        ],
        fence: {kind: "string", rules: [{match: /^\s*(?:```|~~~)\s*$/, pop: true}]},
        html_comment: {kind: "comment", rules: [{match: /-->/, pop: true}]},
    },
});
