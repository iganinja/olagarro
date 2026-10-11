#include <oui/application.h>
#include <oui/pty.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>

using namespace olagarro;

namespace
{

// Runs the application until `done` says so, or a few seconds have gone.
template <typename Done> void run_until(oui::Application& application, Done done)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{20}};
    while(!done() && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

TEST(OuiPtyTests, PassesOnWhatTheProgramWritesAndHowItEnded)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    std::optional<int> exit_code;
    oui::PtyOptions options;
    options.command = {"cmd.exe", "/d", "/c", "echo", "from", "cmd&", "exit", "3"};
    const auto pty{oui::start_pty(
        application, options,
        {[&output](std::string_view text) { output += text; }, [&exit_code](int code) { exit_code = code; }})};
    run_until(application, [&exit_code] { return exit_code.has_value(); });
    EXPECT_NE(output.find("from cmd"), std::string::npos) << output;
    EXPECT_EQ(exit_code, 3);
}

TEST(OuiPtyTests, RunsAProgramInAPseudoConsole)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    std::optional<int> exit_code;
    oui::PtyOptions options;
    options.command = {"cmd.exe", "/d", "/q", "/k"};
    const auto pty{oui::start_pty(
        application, options,
        {[&output](std::string_view text) { output += text; }, [&exit_code](int code) { exit_code = code; }})};
    pty->write("echo got hel^lo\r\n");
    run_until(application, [&output] { return output.find("got hello") != std::string::npos; });
    pty->write("exit 5\r");
    run_until(application, [&exit_code] { return exit_code.has_value(); });
    EXPECT_NE(output.find("got hello"), std::string::npos) << output << "what was typed reached it";
    EXPECT_EQ(exit_code, 5);
    EXPECT_FALSE(pty->is_running());
}

TEST(OuiPtyTests, EndsTheProgramWhenDestroyed)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    oui::PtyOptions options;
    options.command = {"cmd.exe", "/d", "/q", "/k"};
    auto pty{oui::start_pty(application, options, {[&output](std::string_view text) { output += text; }, {}})};
    pty->write("echo started\r");
    run_until(application, [&output] { return output.find("started") != std::string::npos; });
    const auto stopping{std::chrono::steady_clock::now()};
    pty.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - stopping, std::chrono::seconds{5});
}

} // namespace
