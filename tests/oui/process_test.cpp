#include <oui/application.h>
#include <oui/file_system.h>
#include <oui/process.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

using namespace olagarro;

namespace
{

#ifdef _WIN32
constexpr std::string_view line_end{"\r\n"};
#else
constexpr std::string_view line_end{"\n"};
#endif

// What a process told, gathered on the application's thread.
struct Told
{
    std::string output;
    std::string error;
    std::optional<int> exit_code;
};

[[nodiscard]] oui::ProcessCallbacks gather(Told& told)
{
    return {[&told](std::string_view text) { told.output += text; }, [&told](std::string_view text)
            { told.error += text; }, [&told](int exit_code) { told.exit_code = exit_code; }};
}

// Runs the application's frames until the process has ended, or a few seconds have gone.
void run_until_finished(oui::Application& application, const Told& told)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(!told.exit_code && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

TEST(OuiProcessTests, HandsOverBothOutputsAndTheExitCode)
{
    oui::Application application{oui::scripted_backend()};
    Told told;
    const auto process{oui::start_process(
#ifdef _WIN32
        application, {oui::shell_command("echo out&echo err>&2&exit 3"), {}, {}}, gather(told))};
#else
        application, {oui::shell_command("echo out; echo err >&2; exit 3"), {}, {}}, gather(told))};
#endif
    run_until_finished(application, told);
    EXPECT_EQ(told.output, "out" + std::string{line_end});
    EXPECT_EQ(told.error, "err" + std::string{line_end});
    EXPECT_EQ(told.exit_code, 3);
    EXPECT_FALSE(process->is_running());
}

TEST(OuiProcessTests, FeedsTheInputAndRunsInTheFolderAsked)
{
    oui::Application application{oui::scripted_backend()};
    Told told;
    // Larger than a pipe holds, so writing and reading must go together.
#ifdef _WIN32
    std::string input;
    while(input.size() < 200000)
    {
        input += "x" + std::string{line_end};
    }
    // Windows has no cat: find /v "" is how its shell copies its input out.
    const auto process{oui::start_process(application, {oui::shell_command("find /v \"\""), {}, input}, gather(told))};
    run_until_finished(application, told);
    EXPECT_EQ(told.output, input);
#else
    const std::string input(200000, 'x');
    const auto process{oui::start_process(application, {{"wc", "-c"}, {}, input}, gather(told))};
    run_until_finished(application, told);
    EXPECT_EQ(std::stoul(told.output), input.size());
#endif
    EXPECT_EQ(told.exit_code, 0);

    Told where;
    const auto folder{std::filesystem::temp_directory_path()};
#ifdef _WIN32
    const auto pwd{oui::start_process(application, {oui::shell_command("cd"), folder, {}}, gather(where))};
#else
    const auto pwd{oui::start_process(application, {{"pwd", "-P"}, folder, {}}, gather(where))};
#endif
    run_until_finished(application, where);
    EXPECT_EQ(where.output, oui::utf8_of(oui::normal_path(std::filesystem::canonical(folder))) + std::string{line_end});
}

TEST(OuiProcessTests, StopsTheProgramAndWhatItStarted)
{
    oui::Application application{oui::scripted_backend()};
    Told told;
    // The shell waits for a child of its own, which must stop too for the output to end.
#ifdef _WIN32
    const auto process{
        oui::start_process(application, {oui::shell_command("ping -n 30 127.0.0.1 >nul& ver"), {}, {}}, gather(told))};
#else
    const auto process{oui::start_process(application, {oui::shell_command("sleep 30; true"), {}, {}}, gather(told))};
#endif
    ASSERT_TRUE(process->is_running());
    const auto started{std::chrono::steady_clock::now()};
    process->stop();
    run_until_finished(application, told);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds{5});
#ifdef _WIN32
    EXPECT_EQ(told.exit_code, 1) << "the job object ended with 1";
#else
    EXPECT_EQ(told.exit_code, 128 + 15) << "stopped by SIGTERM";
#endif
}

TEST(OuiProcessTests, SaysNothingOnceDestroyed)
{
    oui::Application application{oui::scripted_backend()};
    Told told;
    {
        const auto process{oui::start_process(application, {oui::shell_command("echo hello"), {}, {}}, gather(told))};
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }
    for(int frame{}; frame < 10; ++ frame)
    {
        application.run_frame();
    }
    EXPECT_TRUE(told.output.empty());
    EXPECT_FALSE(told.exit_code.has_value());
}

#ifndef _WIN32
TEST(OuiProcessTests, OpensSomethingWithWhatTheSystemOpensItWithAndLeavesItBe)
{
    // An xdg-open of the test's own, first in PATH, which writes down what it was given once it has run on a while:
    // what it started outlives the call, as a browser outlives the program that asked for it.
    const auto folder{std::filesystem::temp_directory_path() / "oui_open_externally_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    const auto told{folder / "told.txt"};
    {
        auto* const script{oui::open_file(folder / "xdg-open", "wb")};
        ASSERT_NE(script, nullptr);
        const std::string text{"#!/bin/sh\nsleep 0.3\nprintf %s \"$1\" > '" + told.string() + "'\n"};
        std::fwrite(text.data(), 1, text.size(), script);
        std::fclose(script);
        std::filesystem::permissions(folder / "xdg-open", std::filesystem::perms::owner_all);
    }
    const std::string path{std::getenv("PATH") != nullptr ? std::getenv("PATH") : ""};
    ::setenv("PATH", (folder.string() + ":" + path).c_str(), 1);
    const auto asked{oui::open_externally("https://example.org/a page")};
    ::setenv("PATH", path.c_str(), 1);
    EXPECT_TRUE(asked);
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while(!std::filesystem::exists(told) && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_EQ(oui::read_file(told), std::optional<std::string>{"https://example.org/a page"});
    std::filesystem::remove_all(folder);
}
#endif

TEST(OuiProcessTests, FailsAtOnceForAProgramThatDoesNotExist)
{
    oui::Application application{oui::scripted_backend()};
    Told told;
    EXPECT_THROW(
        static_cast<void>(oui::start_process(application, {{"olagarro-no-such-program"}, {}, {}}, gather(told))),
        std::system_error);
}

} // namespace
