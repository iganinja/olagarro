#include <oui/application.h>
#include <oui/file_system.h>
#include <oui/regex.h>
#include <oui/scripted_backend.h>
#include <oui/script/rule_highlighter.h>
#include <oui/script/script_engine.h>
#include <oui/script/system_api.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace olagarro;

namespace
{

using oui::Regex;
using oui::SyntaxKind;
using oui::Utf16Text;
using oui::script::HighlightRule;
using oui::script::HighlightState;
using oui::script::RuleHighlighter;
using Spans = std::vector<std::tuple<std::size_t, std::size_t, SyntaxKind>>;
using Match = std::pair<std::size_t, std::size_t>;

TEST(OuiScriptRegexTests, MatchesAsJavaScriptDoes)
{
    const Regex word{R"(\b[a-z]+\b)", "i"};
    const Utf16Text text{"12 Hello world"};
    EXPECT_EQ(word.search(text.units, 0), (Match{3, 8}));
    EXPECT_EQ(word.search(text.units, 4), (Match{9, 14}));
    EXPECT_FALSE(word.search(text.units, 14).has_value());
    const Regex sticky{"[0-9]+", "y"};
    EXPECT_FALSE(sticky.search(text.units, 2).has_value()) << "sticky matches only where it starts";
    EXPECT_THROW(Regex{"("}, std::invalid_argument);
    EXPECT_THROW((Regex{"a", "q"}), std::invalid_argument);
}

TEST(OuiScriptRegexTests, ReadsUtf8AsUtf16AndBack)
{
    const Utf16Text text{"añ😀b"};
    // a, ñ (two bytes), 😀 (four bytes, two units), b
    EXPECT_EQ(text.units.size(), 5u);
    EXPECT_EQ(text.byte_offsets, (std::vector<std::size_t>{0, 1, 3, 3, 7, 8}));
    const Regex emoji{R"(\u{1F600})", "u"};
    EXPECT_EQ(emoji.search(text.units, 0), (Match{2, 4}));
}

[[nodiscard]] RuleHighlighter c_like()
{
    return RuleHighlighter{{
        {"start",
         SyntaxKind::plain,
         {
             {R"(//.*)", {}, SyntaxKind::comment, {}, false},
             {R"(/\*)", {}, std::nullopt, "block_comment", false},
             {R"(")", {}, std::nullopt, "string", false},
             {R"(\b(if|return)\b)", {}, SyntaxKind::keyword, {}, false},
             {R"(\b[0-9]+\b)", {}, SyntaxKind::number, {}, false},
         }},
        {"block_comment", SyntaxKind::comment, {{R"(\*/)", {}, std::nullopt, {}, true}}},
        {"string", SyntaxKind::string, {{R"(\\.)", {}, std::nullopt, {}, false}, {R"(")", {}, std::nullopt, {}, true}}},
    }};
}

[[nodiscard]] Spans spans_of(const RuleHighlighter& highlighter, std::string_view line,
                             oui::SyntaxHighlighter::State& state)
{
    std::vector<oui::SyntaxSpan> spans;
    state = highlighter.highlight_line(line, state, spans);
    Spans result;
    for(const auto& span : spans)
    {
        result.emplace_back(span.start, span.end, span.kind);
    }
    return result;
}

TEST(OuiScriptRuleHighlighterTests, ColoursByTheRuleThatMatchesFirst)
{
    const auto highlighter{c_like()};
    oui::SyntaxHighlighter::State state{};
    EXPECT_EQ(spans_of(highlighter, "if x return 42; // done", state), (Spans{{0, 2, SyntaxKind::keyword},
                                                                              {5, 11, SyntaxKind::keyword},
                                                                              {12, 14, SyntaxKind::number},
                                                                              {16, 23, SyntaxKind::comment}}));
    EXPECT_EQ(state, 0u);
    EXPECT_EQ(spans_of(highlighter, R"(s = "a\"b" if)", state),
              (Spans{{4, 10, SyntaxKind::string}, {11, 13, SyntaxKind::keyword}}))
        << "an escaped quote stays in the string";
}

TEST(OuiScriptRuleHighlighterTests, CarriesStatesFromLineToLine)
{
    const auto highlighter{c_like()};
    oui::SyntaxHighlighter::State state{};
    EXPECT_EQ(spans_of(highlighter, "1 /* open", state),
              (Spans{{0, 1, SyntaxKind::number}, {2, 9, SyntaxKind::comment}}));
    EXPECT_NE(state, 0u);
    EXPECT_EQ(spans_of(highlighter, "if 2", state), (Spans{{0, 4, SyntaxKind::comment}})) << "still inside";
    EXPECT_EQ(spans_of(highlighter, "*/ if", state), (Spans{{0, 2, SyntaxKind::comment}, {3, 5, SyntaxKind::keyword}}));
    EXPECT_EQ(state, 0u);
}

TEST(OuiScriptRuleHighlighterTests, RefusesAMissingStateAndSurvivesEmptyMatches)
{
    EXPECT_THROW((RuleHighlighter{{{"start", SyntaxKind::plain, {{"x", {}, std::nullopt, "nowhere", false}}}}}),
                 std::invalid_argument);
    // A pattern that can match nothing must not hold the line in place.
    const RuleHighlighter loose{{{"start", SyntaxKind::plain, {{"a*", {}, SyntaxKind::keyword, {}, false}}}}};
    oui::SyntaxHighlighter::State state{};
    EXPECT_EQ(spans_of(loose, "baab", state), (Spans{{1, 3, SyntaxKind::keyword}}));
    // States that enter each other without moving on stop rather than spin.
    const RuleHighlighter circle{{{"a", SyntaxKind::plain, {{"(?=x)", {}, std::nullopt, "b", false}}},
                                  {"b", SyntaxKind::plain, {{"(?=x)", {}, std::nullopt, "a", false}}}}};
    EXPECT_NO_THROW(static_cast<void>(spans_of(circle, "xyz", state)));
}

TEST(OuiScriptEngineTests, RunsScriptsThatCallWhatCxxDefines)
{
    oui::script::ScriptEngine engine;
    std::vector<std::string> told;
    engine.define("app.tell",
                  [&told](const std::vector<oui::script::Value>& arguments)
                  {
                      told.push_back(arguments.at(0).to_string());
                      return oui::script::Value{};
                  });
    std::string printed;
    engine.set_print([&printed](std::string_view text) { printed += text; });
    engine.run(R"(app.tell("hello " + (40 + 2)); console.log("a", 1, [2, 3]);)", "first.js");
    EXPECT_EQ(told, (std::vector<std::string>{"hello 42"}));
    EXPECT_EQ(printed, "a 1 2,3");
}

TEST(OuiScriptEngineTests, ReadsValuesAndCallsBackIntoScripts)
{
    oui::script::ScriptEngine engine;
    oui::script::Value kept;
    engine.define("keep",
                  [&kept](const std::vector<oui::script::Value>& arguments)
                  {
                      kept = arguments.at(0);
                      return oui::script::Value{};
                  });
    engine.run(R"(keep({name: "cpp", list: [".h", ".cpp"], pattern: /a+/gi, flag: true,
                         shout: text => text.toUpperCase()});)",
               "values.js");
    EXPECT_EQ(kept.keys(), (std::vector<std::string>{"name", "list", "pattern", "flag", "shout"}));
    EXPECT_EQ(kept.get("name").to_string(), "cpp");
    ASSERT_TRUE(kept.get("list").is_array());
    EXPECT_EQ(kept.get("list").length(), 2u);
    EXPECT_EQ(kept.get("list").at(1).to_string(), ".cpp");
    EXPECT_TRUE(kept.get("pattern").is_regexp());
    EXPECT_EQ(kept.get("pattern").get("source").to_string(), "a+");
    EXPECT_EQ(kept.get("pattern").get("flags").to_string(), "gi");
    EXPECT_TRUE(kept.get("flag").to_bool());
    EXPECT_TRUE(kept.get("missing").is_undefined());
    EXPECT_EQ(kept.get("shout").call({engine.string("quiet")}).to_string(), "QUIET");
}

TEST(OuiScriptEngineTests, ReportsWhereAScriptFailed)
{
    oui::script::ScriptEngine engine;
    try
    {
        engine.run("let x = 1;\nundefinedFunction();", "broken.js");
        FAIL() << "the script should have thrown";
    }
    catch(const oui::script::ScriptError& error)
    {
        const std::string message{error.what()};
        EXPECT_NE(message.find("undefinedFunction"), std::string::npos) << message;
        EXPECT_NE(message.find("broken.js:2"), std::string::npos) << message;
    }
    EXPECT_THROW(engine.run("this is not javascript", "syntax.js"), oui::script::ScriptError);
    engine.define("fail", [](const std::vector<oui::script::Value>&) -> oui::script::Value
                  { throw oui::script::ScriptError{"no way"}; });
    engine.run(R"(let caught = ""; try { fail(); } catch(e) { caught = e.message; } if(caught !== "no way") throw 1;)",
               "catch.js");
}

TEST(OuiScriptEngineTests, StopsAScriptThatRunsTooLong)
{
    oui::script::ScriptEngine engine;
    engine.set_time_limit(std::chrono::milliseconds{100});
    const auto started{std::chrono::steady_clock::now()};
    EXPECT_THROW(engine.run("while(true) {}", "forever.js"), oui::script::ScriptError);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds{2});
    engine.run("let fine = 1;", "after.js");
}

// Once a script had ended itself, the engine took every uncatchable error after for a stop too: a script that ran too
// long later ended in silence, as though it had stopped on purpose.
TEST(OuiScriptEngineTests, AStopIsForgottenOnceItHasEnded)
{
    oui::script::ScriptEngine engine;
    engine.set_time_limit(std::chrono::milliseconds{100});
    engine.define("stop", [](const std::vector<oui::script::Value>&) -> oui::script::Value
                  { throw oui::script::ScriptStop{}; });
    EXPECT_THROW(engine.run("stop();", "stop.js"), oui::script::ScriptStop);
    try
    {
        engine.run("while(true) {}", "forever.js");
        ADD_FAILURE() << "it ran for ever";
    }
    catch(const oui::script::ScriptStop&)
    {
        ADD_FAILURE() << "taken for a stop";
    }
    catch(const oui::script::ScriptError&)
    {
    }
}

TEST(OuiScriptEngineTests, SettlesPromisesBeforeARunReturns)
{
    oui::script::ScriptEngine engine;
    std::string told;
    engine.define("tell",
                  [&told](const std::vector<oui::script::Value>& arguments)
                  {
                      told = arguments.at(0).to_string();
                      return oui::script::Value{};
                  });
    engine.run(R"((async () => { const value = await Promise.resolve("later"); tell(value); })();)", "async.js");
    EXPECT_EQ(told, "later");
}

TEST(OuiScriptEngineTests, LetsScriptsAwaitWhatCxxSettlesLater)
{
    oui::script::ScriptEngine engine;
    std::optional<oui::script::ScriptEngine::Promise> pending;
    engine.define("later",
                  [&engine, &pending](const std::vector<oui::script::Value>&)
                  {
                      pending = engine.promise();
                      return pending->promise;
                  });
    std::string told;
    engine.define("tell",
                  [&told](const std::vector<oui::script::Value>& arguments)
                  {
                      told = arguments.at(0).to_string();
                      return oui::script::Value{};
                  });
    std::string unhandled;
    engine.set_unhandled_rejection([&unhandled](std::string_view error) { unhandled = error; });
    engine.run(R"((async () => { tell("waiting"); tell("got " + await later()); })();)", "await.js");
    EXPECT_EQ(told, "waiting");
    ASSERT_TRUE(pending.has_value());
    pending->resolve.call({engine.number(7)});
    EXPECT_EQ(told, "got 7") << "settling runs what awaited";

    engine.run(R"((async () => { await later(); throw new Error("after the wait"); })();)", "throws.js");
    pending->resolve.call({});
    EXPECT_NE(unhandled.find("after the wait"), std::string::npos) << unhandled;
    unhandled.clear();
    engine.run(R"(const p = Promise.reject(new Error("caught later")); p.catch(() => {});)", "caught.js");
    EXPECT_TRUE(unhandled.empty()) << "a rejection caught before the work ends is not told: " << unhandled;
}

TEST(OuiScriptEngineTests, RunsModulesThatImportTheirNeighbours)
{
    const auto folder{std::filesystem::temp_directory_path() / "oui_script_modules_test"};
    std::filesystem::create_directories(folder / "lib");
    std::ofstream{folder / "lib" / "maths.js"} << "export const twice = (n) => n * 2;\n";
    std::ofstream{folder / "lib" / "greet.js"} << "import {twice} from './maths.js';\n"
                                                  "export function greet(name) { return name + ' ' + twice(21); }\n";
    oui::script::ScriptEngine engine;
    std::string printed;
    engine.set_print([&printed](std::string_view text) { printed += text; });
    engine.run_module("import {greet} from './lib/greet.js';\nconsole.log(greet('hello'));\n", folder / "main.js");
    EXPECT_EQ(printed, "hello 42");
    // A missing module, or one that fails, fails the run that imported it.
    EXPECT_THROW(engine.run_module("import {nothing} from './missing.js';", folder / "other.js"),
                 oui::script::ScriptError);
    std::ofstream{folder / "throws.js"} << "throw new Error('broken on import');\n";
    try
    {
        engine.run_module("import './throws.js';", folder / "third.js");
        FAIL() << "the import should have failed";
    }
    catch(const oui::script::ScriptError& error)
    {
        EXPECT_NE(std::string{error.what()}.find("broken on import"), std::string::npos) << error.what();
    }
    std::filesystem::remove_all(folder);
}

} // namespace

namespace
{

// A script run live, again and again as it is typed, only looks: it reads, but neither writes, nor runs programs,
// nor asks questions.
TEST(OuiSystemApiTests, AReadOnlyScriptReadsButNeitherWritesNorRuns)
{
    const auto folder{std::filesystem::temp_directory_path() / "oui_read_only_script_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::ofstream{folder / "in.txt"} << "kept";
    oui::Application application{oui::scripted_backend()};
    oui::script::ScriptEngine engine;
    oui::script::SystemApi::Hooks hooks;
    hooks.read_only = true;
    oui::script::SystemApi system{engine, application, hooks};
    std::vector<std::string> printed;
    engine.set_print([&printed](std::string_view text) { printed.emplace_back(text); });
    // As text a path has '/' between folders, which a script's string takes as it is: '\' would start escapes.
    const auto in{oui::utf8_of(folder / "in.txt")};
    const auto out{oui::utf8_of(folder / "out.txt")};
    engine.run("console.log(__ouiSystem.readFile(" + std::string{"\""} + in + "\"));", "read.js");
    EXPECT_EQ(printed, (std::vector<std::string>{"kept"}));
    EXPECT_THROW(engine.run("__ouiSystem.writeFile(\"" + out + "\", \"lost\");", "write.js"), oui::script::ScriptError);
    EXPECT_FALSE(std::filesystem::exists(folder / "out.txt")) << "nothing written";
    EXPECT_THROW(engine.run("__ouiSystem.run(\"touch " + out + "\");", "run.js"), oui::script::ScriptError);
    EXPECT_THROW(engine.run("__ouiSystem.confirm(\"sure?\");", "ask.js"), oui::script::ScriptError);
    std::filesystem::remove_all(folder);
}

} // namespace
