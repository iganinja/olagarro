#include "scripting.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace olagarro;

namespace
{

using oui::SyntaxKind;

// The built-in scripts as OUI Code loads them, read from the sources.
class BuiltInScripts
{
  public:
    BuiltInScripts()
    {
        const std::filesystem::path folder{std::string{OLAGARRO_SOURCE_DIR} + "/examples/oui_code/scripts"};
        for(const auto* const name :
            {"prelude.js", "languages/cpp.js", "languages/javascript.js", "languages/python.js",
             "languages/gdscript.js", "languages/json.js", "languages/markdown.js", "languages/cmake.js",
             "languages/shell.js", "languages/oui.js", "languages/others.js", "commands.js"})
        {
            std::ifstream file{folder / name, std::ios::binary};
            std::ostringstream text;
            text << file.rdbuf();
            m_sources.push_back(text.str());
            m_names.emplace_back(name);
        }
    }
    [[nodiscard]] std::vector<Scripting::Script> scripts() const
    {
        std::vector<Scripting::Script> result;
        for(std::size_t index{}; index < m_sources.size(); ++ index)
        {
            result.push_back({m_names[index], m_sources[index]});
        }
        return result;
    }

  private:
    std::vector<std::string> m_sources;
    std::vector<std::string> m_names;
};

// The text of each span of `line`, with its kind, as `language` colours it from the start of a file.
[[nodiscard]] std::vector<std::pair<std::string, SyntaxKind>>
coloured(const Scripting& scripting, const std::string& file, const std::vector<std::string>& lines)
{
    const auto language{scripting.languages().for_path(file)};
    EXPECT_NE(language, nullptr) << file;
    std::vector<std::pair<std::string, SyntaxKind>> result;
    if(language == nullptr || language->highlighter == nullptr)
    {
        return result;
    }
    oui::SyntaxHighlighter::State state{};
    for(const auto& line : lines)
    {
        std::vector<oui::SyntaxSpan> spans;
        state = language->highlighter->highlight_line(line, state, spans);
        for(const auto& span : spans)
        {
            result.emplace_back(line.substr(span.start, span.end - span.start), span.kind);
        }
    }
    return result;
}

TEST(OuiCodeScriptingTests, LoadsEveryBuiltInScript)
{
    Scripting scripting{"en"};
    const auto problems{scripting.load(BuiltInScripts{}.scripts(), std::nullopt)};
    EXPECT_TRUE(problems.empty()) << (problems.empty() ? "" : problems.front());
    for(const auto* const file : {"a.cpp", "a.h", "a.js", "a.ts", "a.py", "a.gd", "a.json", "README.md", "a.cmake",
                                  "CMakeLists.txt", "a.sh", ".bashrc"})
    {
        EXPECT_NE(scripting.languages().for_path(file), nullptr) << file;
    }
    EXPECT_EQ(scripting.languages().for_path("NOTES.TXT"), nullptr);
    EXPECT_EQ(scripting.languages().for_path("x/Main.CPP")->comment, "//");
}

TEST(OuiCodeScriptingTests, ColoursCxx)
{
    Scripting scripting{"en"};
    static_cast<void>(scripting.load(BuiltInScripts{}.scripts(), std::nullopt));
    using Spans = std::vector<std::pair<std::string, SyntaxKind>>;
    EXPECT_EQ(coloured(scripting, "a.cpp", {"#include <vector> // list"}),
              (Spans{{"#include ", SyntaxKind::preprocessor},
                     {"<vector>", SyntaxKind::string},
                     {" ", SyntaxKind::preprocessor},
                     {"// list", SyntaxKind::comment}}));
    EXPECT_EQ(coloured(scripting, "a.cpp", {R"(if(x) return "a\"b" + 1.5f; /* open)", "still */ int y;"}),
              (Spans{{"if", SyntaxKind::keyword},
                     {"return", SyntaxKind::keyword},
                     {R"("a\"b")", SyntaxKind::string},
                     {"1.5f", SyntaxKind::number},
                     {"/* open", SyntaxKind::comment},
                     {"still */", SyntaxKind::comment},
                     {"int", SyntaxKind::type}}));
}

TEST(OuiCodeScriptingTests, ColoursPythonAndMarkdownAcrossLines)
{
    Scripting scripting{"en"};
    static_cast<void>(scripting.load(BuiltInScripts{}.scripts(), std::nullopt));
    using Spans = std::vector<std::pair<std::string, SyntaxKind>>;
    EXPECT_EQ(coloured(scripting, "a.py", {"def f(): '''doc", "more''' # done"}),
              (Spans{{"def", SyntaxKind::keyword},
                     {"f", SyntaxKind::function},
                     {"'''doc", SyntaxKind::string},
                     {"more'''", SyntaxKind::string},
                     {"# done", SyntaxKind::comment}}));
    EXPECT_EQ(coloured(scripting, "a.md", {"# Title", "```", "# not a title", "```"}),
              (Spans{{"# Title", SyntaxKind::heading},
                     {"```", SyntaxKind::string},
                     {"# not a title", SyntaxKind::string},
                     {"```", SyntaxKind::string}}));
}

TEST(OuiCodeScriptingTests, RunsTextCommandsInTheInterfaceLanguage)
{
    Scripting scripting{"es"};
    static_cast<void>(scripting.load(BuiltInScripts{}.scripts(), std::nullopt));
    const auto& commands{scripting.commands()};
    const auto find{[&commands](std::string_view id) -> const TextCommand&
                    {
                        for(const auto& command : commands)
                        {
                            if(command.id == id)
                            {
                                return command;
                            }
                        }
                        throw std::logic_error{"no command " + std::string{id}};
                    }};
    EXPECT_EQ(find("text.sort_lines").title, "Ordenar líneas");
    EXPECT_EQ(scripting.run(find("text.sort_lines"), "pear\napple\nfig\n"), "apple\nfig\npear\n");
    EXPECT_EQ(scripting.run(find("text.unique_lines"), "a\nb\na"), "a\nb");
    EXPECT_EQ(scripting.run(find("text.snake_case"), "someValue other-name"), "some_value other_name");
    EXPECT_EQ(scripting.run(find("text.format_json"), R"({"a":[1,2]})"),
              "{\n    \"a\": [\n        1,\n        2\n    ]\n}\n");
    EXPECT_THROW(static_cast<void>(scripting.run(find("text.format_json"), "not json")), oui::script::ScriptError);
}

TEST(OuiCodeScriptingTests, RunsTheUsersScriptsAndSaysWhichFailed)
{
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_scripting_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::ofstream{folder / "a_good.js"} << R"(oui.commands.register({id: "user.shout", title: "Shout",
        keys: "Ctrl+Alt+U", run: text => text.toUpperCase() + "!"});
        console.log("loaded");)";
    std::ofstream{folder / "b_bad.js"} << "oui.languages.register({id: 'broken', states: {start: [{match: /(/}]}});";
    std::ofstream{folder / "c_language.js"} << R"(oui.languages.register({id: "cpp", extensions: [".cpp"],
        comment: "#"});)";
    std::ofstream{folder / "notes.txt"} << "not a script";
    Scripting scripting{"en"};
    const auto problems{scripting.load(BuiltInScripts{}.scripts(), folder)};
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems.front().rfind("b_bad.js: ", 0), 0u) << problems.front();
    EXPECT_EQ(scripting.output(), (std::vector<std::string>{"loaded"}));
    const auto& shout{scripting.commands().back()};
    EXPECT_EQ(shout.id, "user.shout");
    ASSERT_TRUE(shout.keys.has_value());
    EXPECT_EQ(oui::format_shortcut(*shout.keys), "Ctrl+Alt+U");
    EXPECT_EQ(scripting.run(shout, "hey"), "HEY!");
    EXPECT_EQ(scripting.languages().for_path("a.cpp")->comment, "#")
        << "the user's definition replaces the built-in one";
    std::filesystem::remove_all(folder);
}

TEST(OuiCodeScriptingTests, ReadsWhereATextCommandGoesInTheMenus)
{
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_menu_path_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::ofstream{folder / "a.js"} << R"(
        oui.commands.register({id: "user.one", title: "One", menuPath: "Edit/&Case/Deeper", run: text => text});
        oui.commands.register({id: "user.two", title: "Two", menuPath: {en: "tools/Case", es: "tools/Mayúsculas"},
                               run: text => text});
        oui.commands.register({id: "user.three", title: "Three", run: text => text});)";
    std::ofstream{folder / "b.js"} << R"(oui.commands.register({id: "user.bad", title: "Bad", menuPath: "nowhere/x",
        run: text => text});)";
    std::ofstream{folder / "c.js"} << R"(oui.commands.register({id: "user.gap", title: "Gap", menuPath: "edit//x",
        run: text => text});)";
    Scripting scripting{"es"};
    const auto problems{scripting.load({}, folder)};
    ASSERT_EQ(problems.size(), 2u);
    // Each says the script and what is wrong, then where, as JavaScript says it.
    EXPECT_EQ(problems[0].rfind("b.js: TypeError: command user.bad: menuPath \"nowhere/x\" starts with no menu of OUI "
                                "Code's: file, edit, view, tools or help\n",
                                0),
              0u)
        << problems[0];
    EXPECT_EQ(problems[1].rfind("c.js: TypeError: command user.gap: menuPath \"edit//x\" has a name missing, such as "
                                "\"edit/Case\"\n",
                                0),
              0u)
        << problems[1];
    const auto& commands{scripting.commands()};
    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0].menu_path, (std::vector<std::string>{"edit", "&Case", "Deeper"}));
    EXPECT_EQ(commands[1].menu_path, (std::vector<std::string>{"tools", "Mayúsculas"}));
    EXPECT_TRUE(commands[2].menu_path.empty());
    std::filesystem::remove_all(folder);
}

} // namespace

TEST(OuiCodeScriptingTests, ColoursUserInterfaceDescriptions)
{
    Scripting scripting{"en"};
    static_cast<void>(scripting.load(BuiltInScripts{}.scripts(), std::nullopt));
    using Spans = std::vector<std::pair<std::string, SyntaxKind>>;
    EXPECT_EQ(coloured(scripting, "find.oui",
                       {"# What to find.", R"(Window "Find # it" context=FindInFiles width=60)",
                        R"(  Button "OK" id=ok default)"}),
              (Spans{{"# What to find.", SyntaxKind::comment},
                     {"Window", SyntaxKind::type},
                     {R"("Find # it")", SyntaxKind::string},
                     {"context", SyntaxKind::attribute},
                     {"FindInFiles", SyntaxKind::constant},
                     {"width", SyntaxKind::attribute},
                     {"60", SyntaxKind::number},
                     {"Button", SyntaxKind::type},
                     {R"("OK")", SyntaxKind::string},
                     {"id", SyntaxKind::attribute},
                     {"ok", SyntaxKind::constant},
                     {"default", SyntaxKind::attribute}}));
}
