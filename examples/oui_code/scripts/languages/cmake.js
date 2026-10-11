// CMake.
oui.languages.register({
    id: "cmake",
    name: "CMake",
    extensions: [".cmake"],
    files: ["CMakeLists.txt"],
    comment: "#",
    states: {
        start: [
            {match: /#\[\[/, push: "block_comment"},
            {match: /#.*/, kind: "comment"},
            {match: /"/, push: "string"},
            {match: /\$\{[^}]*\}|\$ENV\{[^}]*\}|\$<[^>]*>/, kind: "constant"},
            {match: /\b(?:if|elseif|else|endif|foreach|endforeach|while|endwhile|function|endfunction|macro|endmacro|return|break|continue)\b(?=\s*\()/i, kind: "keyword"},
            {match: /\b[A-Za-z_]\w*(?=\s*\()/, kind: "function"},
            {match: /\b[A-Z][A-Z0-9_]{2,}\b/, kind: "constant"},
            {match: /\b\d+(?:\.\d+)*\b/, kind: "number"},
        ],
        block_comment: {kind: "comment", rules: [{match: /\]\]/, pop: true}]},
        string: {
            kind: "string",
            rules: [{match: /\\./}, {match: /\$\{[^}]*\}/, kind: "constant"}, {match: /"/, pop: true}],
        },
    },
});
