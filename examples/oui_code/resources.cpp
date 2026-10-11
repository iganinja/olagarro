#include "resources.h"

#include <array>

using namespace olagarro;

std::string_view user_script_template()
{
    return R"(// Your own commands and languages for OUI Code, run after its own scripts. Every .js file in this
// folder runs, in name order; Tools > User Scripts > Reload User Scripts runs them again after a change.
//
// A command gets the selection, or the whole document when nothing is selected, and gives back what goes in
// its place; it appears in Tools > Text Commands, or where menuPath says, and the command palette.
//
// oui.commands.register({
//     id: "user.shout",
//     title: "Shout",
//     keys: "Ctrl+Alt+U",
//     menuPath: "edit/Case",
//     run: text => text.toUpperCase(),
// });
//
// A language: its files, what starts a comment, and how to colour it. Rules are tried state by state; the
// one that matches first wins. Kinds: keyword, type, function, string, number, constant, comment,
// preprocessor, operator, tag, attribute, heading, emphasis, link, invalid.
//
// oui.languages.register({
//     id: "ini",
//     name: "INI",
//     extensions: [".ini"],
//     comment: ";",
//     states: {
//         start: [
//             {match: /;.*/, kind: "comment"},
//             {match: /^\s*\[.*\]/, kind: "heading"},
//             {match: /^[^=]+(?==)/, kind: "attribute"},
//             {match: /"/, push: "string"},
//         ],
//         string: {kind: "string", rules: [{match: /\\./}, {match: /"/, pop: true}]},
//     },
// });
)";
}

std::vector<Scripting::Script> built_in_scripts()
{
    static constexpr std::array names{"prelude.js",
                                      "languages/cpp.js",
                                      "languages/javascript.js",
                                      "languages/python.js",
                                      "languages/gdscript.js",
                                      "languages/json.js",
                                      "languages/markdown.js",
                                      "languages/cmake.js",
                                      "languages/shell.js",
                                      "languages/oui.js",
                                      "languages/others.js",
                                      "commands.js"};
    const auto sources{embedded_oui_code_scripts()};
    std::vector<Scripting::Script> scripts;
    for(std::size_t index{}; index < sources.size(); ++ index)
    {
        scripts.push_back({names.at(index), sources[index]});
    }
    return scripts;
}

const std::vector<oui::Translation>& oui_code_translations()
{
    static const std::vector<oui::Translation> translations{oui::embedded_translations(embedded_oui_code_catalogs())};
    return translations;
}
