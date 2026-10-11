// Languages OUI Code knows only the comments of, for Toggle Line Comment; no colours yet.
{
    const comment_only = [
        {id: "csharp", name: "C#", extensions: [".cs"], comment: "//"},
        {id: "java", name: "Java", extensions: [".java"], comment: "//"},
        {id: "go", name: "Go", extensions: [".go"], comment: "//"},
        {id: "rust", name: "Rust", extensions: [".rs"], comment: "//"},
        {id: "swift", name: "Swift", extensions: [".swift"], comment: "//"},
        {id: "kotlin", name: "Kotlin", extensions: [".kt", ".kts"], comment: "//"},
        {id: "shader", name: "Shaders", extensions: [".glsl", ".shader", ".gdshader", ".hlsl", ".vert", ".frag"],
         comment: "//"},
        {id: "ruby", name: "Ruby", extensions: [".rb"], comment: "#"},
        {id: "perl", name: "Perl", extensions: [".pl", ".pm"], comment: "#"},
        {id: "yaml", name: "YAML", extensions: [".yaml", ".yml"], comment: "#"},
        {id: "toml", name: "TOML", extensions: [".toml"], comment: "#"},
        {id: "config", name: "Configuration", extensions: [".conf", ".cfg", ".ini"],
         files: [".gitignore", ".gitattributes", ".editorconfig"], comment: "#"},
        {id: "make", name: "Makefile", extensions: [".mk"], files: ["Makefile", "GNUmakefile", "makefile"],
         comment: "#"},
        {id: "docker", name: "Dockerfile", files: ["Dockerfile"], comment: "#"},
        {id: "r", name: "R", extensions: [".r"], comment: "#"},
        {id: "lua", name: "Lua", extensions: [".lua"], comment: "--"},
        {id: "sql", name: "SQL", extensions: [".sql"], comment: "--"},
        {id: "haskell", name: "Haskell", extensions: [".hs"], comment: "--"},
    ];
    for (const language of comment_only) {
        oui.languages.register(language);
    }
}
