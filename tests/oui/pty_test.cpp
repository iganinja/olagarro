#include <oui/application.h>
#include <oui/pty.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include <signal.h>

using namespace olagarro;

namespace
{

// Runs the application until `done` says so, or a few seconds have gone.
template <typename Done> void run_until(oui::Application& application, Done done)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(!done() && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

TEST(OuiPtyTests, RunsAProgramInATerminalOfItsOwn)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    std::optional<int> exit_code;
    oui::PtyOptions options;
    options.command = {"/bin/sh", "-c", "tty; stty size; echo \"$TERM\"; read line; echo \"got $line\"; exit 3"};
    options.columns = 77;
    options.rows = 19;
    const auto pty{oui::start_pty(
        application, options,
        {[&output](std::string_view text) { output += text; }, [&exit_code](int code) { exit_code = code; }})};
    run_until(application, [&output] { return output.find("xterm-256color") != std::string::npos; });
    pty->write("hello\r");
    run_until(application, [&exit_code] { return exit_code.has_value(); });
    EXPECT_NE(output.find("/dev/pts/"), std::string::npos) << output << "a terminal, not a pipe";
    EXPECT_NE(output.find("19 77"), std::string::npos) << output << "the size it was given";
    EXPECT_NE(output.find("got hello"), std::string::npos) << output << "what was typed reached it";
    EXPECT_EQ(exit_code, 3);
    EXPECT_FALSE(pty->is_running());
}

TEST(OuiPtyTests, TellsTheProgramWhenTheTerminalChangesSize)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    oui::PtyOptions options;
    options.command = {"/bin/sh", "-c", "trap 'stty size; exit 0' WINCH; echo ready; while :; do sleep 0.05; done"};
    const auto pty{oui::start_pty(application, options, {[&output](std::string_view text) { output += text; }, {}})};
    run_until(application, [&output] { return output.find("ready") != std::string::npos; });
    pty->resize(100, 30);
    run_until(application, [&output] { return output.find("30 100") != std::string::npos; });
    EXPECT_NE(output.find("30 100"), std::string::npos) << output;
}

TEST(OuiPtyTests, EndsTheProgramWhenDestroyed)
{
    oui::Application application{oui::scripted_backend()};
    std::string output;
    oui::PtyOptions options;
    options.command = {"/bin/sh", "-c", "echo $$; sleep 30"};
    auto pty{oui::start_pty(application, options, {[&output](std::string_view text) { output += text; }, {}})};
    run_until(application, [&output] { return output.find('\n') != std::string::npos; });
    const auto pid{std::stoi(output)};
    const auto started{std::chrono::steady_clock::now()};
    pty.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds{2});
    EXPECT_NE(::kill(pid, 0), 0) << "the shell is gone";
}

} // namespace
