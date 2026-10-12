#include "script_run.h"

#include <oui/file_system.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

using namespace olagarro;

namespace
{

// A folder of its own for each test, with `script` written in it as script.js and a file to run it on.
class ScriptRunTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        folder =
            std::filesystem::temp_directory_path() /
            (std::string{"oui_code_script_run_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::filesystem::create_directories(folder);
        file = folder / oui::path_from_utf8("notes ñ.txt");
        write(file, "hello\n");
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
    }

    static void write(const std::filesystem::path& path, std::string_view text)
    {
        auto* const out{oui::open_file(path, "wb")};
        ASSERT_NE(out, nullptr);
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }

    // Loads `script` as script.js and runs it, on the file, on its own from the folder, or on `code` generated from
    // description.oui in the folder, whichever it is, and waits, for a few seconds at most, for it to end. Says why
    // when it does not load.
    [[nodiscard]] std::optional<std::string> run(std::string_view script, bool dry_run)
    {
        write(folder / "script.js", script);
        ended = false;
        failure.reset();
        returned.reset();
        output.clear();
        ScriptRun runner{application,
                         {[this](std::string_view text) { output += text; },
                          [this](std::optional<std::string> error)
                          {
                              ended = true;
                              failure = std::move(error);
                          }}};
        if(auto problem{runner.load(folder / "script.js")})
        {
            return problem;
        }
        if(runner.kind() == ScriptRun::Kind::on_file)
        {
            runner.run_on_file(file, dry_run);
        }
        else if(runner.kind() == ScriptRun::Kind::on_generated_code)
        {
            runner.run_on_generated_code(code, "cpp", folder / "description.oui");
        }
        else if(runner.kind() == ScriptRun::Kind::tool)
        {
            tool = runner.tool().get("title").to_string();
            runner.run_tool(context);
        }
        else
        {
            runner.run_on_its_own(folder, dry_run);
        }
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
        while(!ended && std::chrono::steady_clock::now() < deadline)
        {
            application.run_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        returned = runner.returned();
        EXPECT_TRUE(ended) << "the run never ended";
        return std::nullopt;
    }

    // Why `script` cannot run, or nothing when it can.
    [[nodiscard]] std::optional<std::string> problem_with(std::string_view script)
    {
        write(folder / "candidate.js", script);
        ScriptRun checker{application, {[](std::string_view) {}, [](std::optional<std::string>) {}}};
        return checker.load(folder / "candidate.js");
    }

    oui::Application application{oui::scripted_backend()};
    std::filesystem::path folder;
    std::filesystem::path file;
    std::string output;
    bool ended{false};
    std::optional<std::string> failure;
    // What the run's function gave back as text.
    std::optional<std::string> returned;
    // What a processGeneratedCode gets.
    std::string code{"// generated\n"};
    // What a tool's run gets, and the title its tool said.
    std::string context{"{file: null, language: \"markdown\", folder: \"/tmp\", selection: \"two words\", line: 3, "
                        "column: 7}"};
    std::string tool;
};

TEST_F(ScriptRunTests, ProcessFileGetsTheFileAndWhetherItIsADryRun)
{
    const auto script{"export function processFile(file, dryRun) {\n"
                      "    console.log(file.endsWith('notes ñ.txt'), dryRun);\n"
                      "    if (!dryRun) oui.writeFile(file, oui.readFile(file).toUpperCase());\n"
                      "}\n"};
    EXPECT_EQ(run(script, true), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(output, "true true\n");
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"hello\n"});

    EXPECT_EQ(run(script, false), std::nullopt);
    EXPECT_EQ(output, "true false\n");
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"HELLO\n"});
}

TEST_F(ScriptRunTests, ProcessRunsOnItsOwnFromAFolder)
{
    const auto script{"export function process(dryRun, folder) {\n"
                      "    const texts = oui.listDir(folder).filter((name) => name.endsWith('.txt'));\n"
                      "    console.log(dryRun ? 'would join' : 'joining', texts.join(','));\n"
                      "    if (!dryRun) oui.writeFile(folder + '/joined.out', texts.join(','));\n"
                      "}\n"};
    EXPECT_EQ(run(script, true), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(output, "would join notes ñ.txt\n");
    EXPECT_FALSE(std::filesystem::exists(folder / "joined.out"));

    EXPECT_EQ(run(script, false), std::nullopt);
    EXPECT_EQ(output, "joining notes ñ.txt\n");
    EXPECT_EQ(oui::read_file(folder / "joined.out"), std::optional<std::string>{"notes ñ.txt"});
}

TEST_F(ScriptRunTests, AnAsyncScriptEndsWhenWhatItAwaitsDoes)
{
#ifdef _WIN32
    constexpr auto command{"echo from the shell"};
#else
    constexpr auto command{"echo 'from the shell'"};
#endif
    EXPECT_EQ(run(std::string{"export async function process(dryRun) {\n"
                              "    const code = await oui.run(\""} +
                      command +
                      "\");\n"
                      "    console.log('exit', code);\n"
                      "}\n",
                  true),
              std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_NE(output.find("from the shell"), std::string::npos) << output;
    EXPECT_NE(output.find("exit 0"), std::string::npos) << output;
}

TEST_F(ScriptRunTests, ProcessGeneratedCodeGetsTheCodeAndWhatItIsForAndItsTextIsKept)
{
    EXPECT_EQ(run("export async function processGeneratedCode(code, {language, file, dryRun}) {\n"
                  "    await null;\n"
                  "    console.log(JSON.stringify(code), language, file.endsWith('description.oui'), dryRun);\n"
                  "    oui.writeFile(file.replace(/\\.oui$/, '.cpp'), code);\n"
                  "    return 'Wrote it.';\n"
                  "}\n",
                  false),
              std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(output, "\"// generated\\n\" cpp true false\n");
    EXPECT_EQ(returned, std::optional<std::string>{"Wrote it."});
    EXPECT_EQ(oui::read_file(folder / "description.cpp"), std::optional<std::string>{"// generated\n"});

    EXPECT_EQ(run("export function processGeneratedCode(code) {}\n", false), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(returned, std::nullopt) << "nothing given back, nothing to show";
}

TEST_F(ScriptRunTests, AToolSaysWhatItIsAndItsRunGetsWhatTheEditorHas)
{
    EXPECT_EQ(run("export const tool = {title: 'Count Words', keys: 'Ctrl+Alt+W'};\n"
                  "export async function run({file, language, selection, line, column}) {\n"
                  "    await null;\n"
                  "    console.log(file, language, selection.split(' ').length, line, column);\n"
                  "    return selection.toUpperCase();\n"
                  "}\n",
                  false),
              std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(tool, "Count Words");
    EXPECT_EQ(output, "null markdown 2 3 7\n");
    EXPECT_EQ(returned, std::optional<std::string>{"TWO WORDS"});

    EXPECT_EQ(run("export const tool = {title: 'Quiet'};\nexport function run(context) { return null; }\n", false),
              std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(returned, std::nullopt) << "null, as undefined, leaves the document";
}

TEST_F(ScriptRunTests, AToolNeedsItsRunAndAnObjectToSayWhatItIs)
{
    const auto without_run{problem_with("export const tool = {title: 'Nothing'};\n")};
    ASSERT_TRUE(without_run.has_value());
    EXPECT_NE(without_run->find("run(context)"), std::string::npos) << *without_run;
    const auto not_an_object{problem_with("export const tool = 'Count';\nexport function run() {}\n")};
    ASSERT_TRUE(not_an_object.has_value());
    EXPECT_NE(not_an_object->find("must be an object"), std::string::npos) << *not_an_object;
    const auto both{problem_with("export const tool = {};\nexport function run() {}\n"
                                 "export function process(dryRun) {}\n")};
    ASSERT_TRUE(both.has_value());
    EXPECT_NE(both->find("more than one"), std::string::npos) << *both;
}

TEST_F(ScriptRunTests, ImportsResolveFromTheScriptsFolder)
{
    write(folder / "helper.js", "export const shout = (text) => text + '!';\n");
    EXPECT_EQ(run("import {shout} from './helper.js';\n"
                  "export function process(dryRun) { console.log(shout('hi')); }\n",
                  true),
              std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(output, "hi!\n");
}

TEST_F(ScriptRunTests, SaysWhatWentWrongInTheScript)
{
    EXPECT_EQ(run("export async function processFile(file, dryRun) {\n"
                  "    await null;\n"
                  "    throw new Error('broken on purpose');\n"
                  "}\n",
                  true),
              std::nullopt);
    ASSERT_TRUE(failure);
    EXPECT_NE(failure->find("broken on purpose"), std::string::npos) << *failure;
    EXPECT_NE(failure->find("script.js:3"), std::string::npos) << *failure;
}

TEST_F(ScriptRunTests, LoadingTellsWhatCannotRunWithoutRunningAnything)
{
    EXPECT_EQ(problem_with("export function processFile(file, dryRun) { oui.writeFile(file, 'touched'); }\n"),
              std::nullopt);
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"hello\n"}) << "loading calls nothing";
    EXPECT_EQ(problem_with("export const process = async (dryRun, folder) => {};\n"), std::nullopt);

    const auto expect_problem{[this](std::string_view script, std::string_view mentioned)
                              {
                                  const auto problem{problem_with(script)};
                                  ASSERT_TRUE(problem) << script;
                                  EXPECT_NE(problem->find(mentioned), std::string::npos) << *problem;
                              }};
    expect_problem("export function main(file) {}\n", "processFile(file, dryRun)");
    expect_problem("export function processFile(file) {}\n", "processFile(file, dryRun)");
    expect_problem("export function processFile(file, dryRun = true) {}\n", "processFile(file, dryRun)");
    expect_problem("export function process() {}\n", "process(dryRun, folder)");
    expect_problem("export function process(dryRun) {}\nexport function processFile(file, dryRun) {}\n",
                   "more than one");
    expect_problem("export function process(dryRun) {}\nexport function processGeneratedCode(code) {}\n",
                   "more than one");
    expect_problem("export function main(file) {}\n", "processGeneratedCode(code, {language, file})");
    expect_problem("export function process( {\n", "candidate.js");
}

TEST_F(ScriptRunTests, TheTemplatesAreScriptsThatRun)
{
    EXPECT_EQ(problem_with(process_file_template), std::nullopt);
    EXPECT_EQ(problem_with(process_template), std::nullopt);
    EXPECT_EQ(problem_with(generated_code_template), std::nullopt);
    // The tool's says what it found in a message, which waits for someone to close it: loaded, not run.
    EXPECT_EQ(problem_with(tool_template), std::nullopt);
    {
        ScriptRun checker{application, {[](std::string_view) {}, [](std::optional<std::string>) {}}};
        ASSERT_EQ(checker.load(folder / "candidate.js"), std::nullopt);
        EXPECT_EQ(checker.kind(), ScriptRun::Kind::tool);
        EXPECT_EQ(checker.tool().get("title").to_string(), "Count Words");
    }

    write(file, "a\tb\n");
    EXPECT_EQ(run(process_file_template, true), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"a\tb\n"}) << "a dry run changes nothing";
    EXPECT_EQ(run(process_file_template, false), std::nullopt);
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"a    b\n"});

    write(file, "trailing   \nspaces \n");
    EXPECT_EQ(run(process_template, true), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_NE(output.find("Would trim notes ñ.txt"), std::string::npos) << output;
    EXPECT_EQ(run(process_template, false), std::nullopt);
    EXPECT_EQ(oui::read_file(file), std::optional<std::string>{"trailing\nspaces\n"});

    EXPECT_EQ(run(generated_code_template, false), std::nullopt);
    EXPECT_EQ(failure, std::nullopt);
    EXPECT_EQ(oui::read_file(folder / "description.cpp"), std::optional<std::string>{"// generated\n"});
    ASSERT_TRUE(returned);
    EXPECT_NE(returned->find("Wrote"), std::string::npos) << *returned;
    code = "// again\n";
    EXPECT_EQ(run(generated_code_template, false), std::nullopt);
    EXPECT_EQ(oui::read_file(folder / "description.cpp"), std::optional<std::string>{"// generated\n"})
        << "what is there already is the user's";
    ASSERT_TRUE(returned);
    EXPECT_NE(returned->find("already"), std::string::npos) << *returned;
}

} // namespace
