// Shell scripts.
oui.languages.register({
    id: "shell",
    name: "Shell",
    extensions: [".sh", ".bash", ".zsh"],
    files: [".bashrc", ".bash_profile", ".profile", ".zshrc"],
    comment: "#",
    states: {
        start: [
            {match: /(?<![\w$#{])#.*/, kind: "comment"},
            {match: /'[^']*'/, kind: "string"},
            {match: /"/, push: "string"},
            {match: /\$\{[^}]*\}|\$\w+|\$[@#?$!*0-9-]/, kind: "constant"},
            {match: oui.words("if", "then", "else", "elif", "fi", "for", "while", "until", "do", "done", "case",
                              "esac", "function", "in", "select", "return", "exit", "export", "local", "readonly",
                              "declare", "set", "unset", "shift", "source"),
             kind: "keyword"},
            {match: /\b[A-Za-z_][\w-]*(?=\s*\(\)\s*\{?)/, kind: "function"},
            {match: /\b\d+\b/, kind: "number"},
        ],
        string: {
            kind: "string",
            rules: [{match: /\\./}, {match: /\$\{[^}]*\}|\$\w+|\$[@#?$!*0-9-]/, kind: "constant"}, {match: /"/, pop: true}],
        },
    },
});
