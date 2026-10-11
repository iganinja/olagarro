#include "live_script.h"
#include "script_run.h"

#include <oui/file_system.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace olagarro;

namespace
{

// The script beside the input, in a folder of their own; each test runs it live and takes what it heard.
class LiveScriptTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        folder = std::filesystem::temp_directory_path() /
                 (std::string{"oui_code_live_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::filesystem::create_directories(folder);
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
    }

    // The results heard, after `code` runs on `input`, once the application has had time to tell them.
    std::vector<LiveResult> run(const std::string& code, std::optional<std::string> input = std::nullopt)
    {
        live.run(code, folder / "live.js", std::move(input), folder / "notes.txt", std::nullopt);
        wait();
        return heard;
    }

    void wait()
    {
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
        const auto before{heard.size()};
        while(heard.size() == before && std::chrono::steady_clock::now() < deadline)
        {
            application.run_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }

    std::filesystem::path folder;
    oui::Application application{oui::scripted_backend()};
    std::vector<LiveResult> heard;
    LiveScript live{application, [this](LiveResult result) { heard.push_back(std::move(result)); }};
};

TEST_F(LiveScriptTests, ATransformGivesTheResultAndWhatItPrinted)
{
    const auto results{run("export function transform(text) {\n"
                           "    console.log(\"lines\", text.split(\"\\n\").length);\n"
                           "    return text.toUpperCase();\n"
                           "}\n",
                           "uno\ndos ñ")};
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].text, "UNO\nDOS Ñ");
    EXPECT_EQ(results[0].log, "lines 2\n");
    EXPECT_FALSE(results[0].error.has_value());
}

TEST_F(LiveScriptTests, AProcessGeneratedCodeGetsTheInputAsTheCodeInADryRunAndGivesItsText)
{
    live.run("export function processGeneratedCode(code, {language, file, dryRun}) {\n"
             "    console.log(code, language, file.endsWith('form.oui'), dryRun);\n"
             "    return `Would write ${code.length} bytes.`;\n"
             "}\n",
             folder / "live.js", "// code", std::nullopt, GeneratedFrom{"cpp", folder / "form.oui"});
    wait();
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_FALSE(heard[0].error.has_value()) << *heard[0].error;
    EXPECT_EQ(heard[0].log, "// code cpp true true\n");
    EXPECT_EQ(heard[0].text, "Would write 7 bytes.");
}

TEST_F(LiveScriptTests, TheTemplateAfterGenerateSaysWhatItWouldDoAndDoesNothing)
{
    live.run(generated_code_template, folder / "live.js", "// code", std::nullopt,
             GeneratedFrom{"cpp", folder / "form.oui"});
    wait();
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_FALSE(heard[0].error.has_value()) << *heard[0].error;
    ASSERT_TRUE(heard[0].text);
    EXPECT_NE(heard[0].text->find("Would write"), std::string::npos) << *heard[0].text;
    EXPECT_FALSE(std::filesystem::exists(folder / "form.cpp"));
}

TEST_F(LiveScriptTests, AnErrorSaysOnWhichLine)
{
    auto results{run("const a = 1;\nconst b = ;\n", "x")};
    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].error.has_value());
    EXPECT_EQ(results[0].error_line, 2u) << *results[0].error;
    results = run("export function transform(text) {\n    return text;\n}\nnothing.here();\n", "x");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[1].error_line, 4u) << results[1].error.value_or("");
}

TEST_F(LiveScriptTests, AProcessFileWorksOnACopyAsADryRun)
{
    const auto results{run("export function processFile(file, dryRun) {\n"
                           "    console.log(\"dry run:\", dryRun);\n"
                           "    oui.writeFile(file, oui.readFile(file) + \"!\");\n"
                           "}\n",
                           "hello")};
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].text, "hello!") << results[0].error.value_or("");
    EXPECT_EQ(results[0].log, "dry run: true\n");
    EXPECT_FALSE(std::filesystem::exists(folder / "notes.txt")) << "the input's own file is not touched";
}

TEST_F(LiveScriptTests, ItWritesNothingElseRunsNothingAndEndsLoops)
{
    auto results{run("oui.writeFile(" + std::string{"\""} + oui::utf8_of(folder / "out.txt") + "\", \"no\");\n")};
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].error.has_value());
    EXPECT_FALSE(std::filesystem::exists(folder / "out.txt"));
    results = run("oui.run(\"true\");\n");
    EXPECT_TRUE(results.back().error.has_value());
    results = run("while(true) {}\n");
    EXPECT_TRUE(results.back().error.has_value()) << "a loop with no end is cut short";
}

TEST_F(LiveScriptTests, WithNothingExportedWhatItPrintsIsAll)
{
    const auto results{run("for(const n of [1, 2]) console.log(n * 10);\n", "ignored")};
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].text.has_value());
    EXPECT_EQ(results[0].log, "10\n20\n");
}

TEST_F(LiveScriptTests, OnlyTheLastRunIsHeard)
{
    live.run("console.log(\"first\");\n", folder / "live.js", std::nullopt, std::nullopt, std::nullopt);
    live.run("console.log(\"second\");\n", folder / "live.js", std::nullopt, std::nullopt, std::nullopt);
    wait();
    for(int frame{}; frame < 50; ++ frame)
    {
        application.run_frame();
    }
    ASSERT_EQ(heard.size(), 1u);
    EXPECT_EQ(heard[0].log, "second\n");
}

} // namespace
