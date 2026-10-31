#include <oui/application.h>
#include <oui/scripted_backend.h>
#include <oui/terminal_view.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace olagarro;

namespace
{

// A backend that keeps the clipboard itself, so the tests never reach the system's.
class ClipboardBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {80, 24}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
    [[nodiscard]] bool keeps_clipboard() const override { return true; }
    [[nodiscard]] std::string clipboard_text() const override { return *m_clipboard; }
    void set_clipboard_text(std::string_view text) override { *m_clipboard = text; }

    std::shared_ptr<std::string> m_clipboard{std::make_shared<std::string>()};
};

[[nodiscard]] oui::MouseEvent press(double x, double y, int clicks = 1)
{
    oui::MouseEvent mouse{x, y, true};
    mouse.clicks = clicks;
    return mouse;
}

[[nodiscard]] oui::MouseEvent drag(double x, double y)
{
    oui::MouseEvent mouse{x, y, false};
    mouse.dragging = true;
    return mouse;
}

[[nodiscard]] oui::MouseEvent release(double x, double y)
{
    oui::MouseEvent mouse{x, y, false};
    mouse.released = true;
    return mouse;
}

template <typename Done> void run_until(oui::Application& application, Done done)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(!done() && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

[[nodiscard]] std::string row_text(const oui::TerminalScreen& screen, int row)
{
    std::string text;
    for(int column{}; column < screen.columns(); ++ column)
    {
        text += static_cast<char>(screen.cell(column, row).glyph);
    }
    return text.substr(0, text.find_last_not_of(' ') + 1);
}

[[nodiscard]] bool shows(const oui::TerminalScreen& screen, const std::string& text)
{
    for(int row{}; row < screen.rows(); ++ row)
    {
        if(row_text(screen, row).find(text) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

void type(oui::TerminalView& view, std::u32string_view text)
{
    for(const auto character : text)
    {
        view.handle_event(oui::TextEvent{character});
    }
    view.handle_event(oui::KeyEvent{oui::Key::enter});
}

TEST(OuiTerminalViewTests, RunsAShellSizedAsTheView)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sh"};
    oui::TerminalView view{application, options};
    std::optional<int> exit_code;
    view.on_finished([&exit_code](int code) { exit_code = code; });
    view.arrange({0, 0, 50, 12});
    type(view, U"stty size; printf '\\033]0;named\\007'; echo do''ne");
    run_until(application, [&view] { return shows(view.screen(), "done"); });
    EXPECT_TRUE(shows(view.screen(), "12 50")) << "the shell was told the view's size";
    EXPECT_EQ(view.title(), "named");
    type(view, U"exit 4");
    run_until(application, [&exit_code] { return exit_code.has_value(); });
    EXPECT_EQ(exit_code, 4);
    EXPECT_FALSE(view.is_running());
}

TEST(OuiTerminalViewTests, ControlCInterruptsTheProgram)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sh"};
    oui::TerminalView view{application, options};
    view.arrange({0, 0, 60, 10});
    type(view, U"sleep 30; echo sl''ept");
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    view.handle_event(oui::TextEvent{U'c', true});
    type(view, U"echo after''wards");
    run_until(application, [&view] { return shows(view.screen(), "afterwards"); });
    EXPECT_TRUE(shows(view.screen(), "afterwards"));
    EXPECT_FALSE(shows(view.screen(), "slept"));
}

// Until a program names the window, it is named after the program that has the terminal.
TEST(OuiTerminalViewTests, IsNamedAfterTheProgramInFront)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sh"};
    oui::TerminalView view{application, options};
    view.arrange({0, 0, 80, 24});
    std::vector<std::string> titles;
    view.on_title_changed([&titles](const std::string& title) { titles.push_back(title); });
    type(view, U"echo ready");
    run_until(application, [&view] { return view.title() == "sh"; });
    EXPECT_EQ(view.title(), "sh");
    // sleep rather than top: top -b ignores SIGHUP, as nohup would, so closing the terminal left it running.
    type(view, U"sleep 30");
    run_until(application, [&view] { return view.title() == "sleep"; });
    EXPECT_EQ(view.title(), "sleep");
}

// The title was looked at only as output came: a program that writes nothing, or that starts after the shell has
// written the command typed, left it as it was.
TEST(OuiTerminalViewTests, IsNamedAfterAProgramThatWritesNothing)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sleep", "30"};
    oui::TerminalView view{application, options};
    view.arrange({0, 0, 80, 24});
    run_until(application, [&view] { return view.title() == "sleep"; });
    EXPECT_EQ(view.title(), "sleep");
}

// What the mouse selects goes to the clipboard: dragged, a word with a double click, a line with a triple one.
TEST(OuiTerminalViewTests, TheMouseSelectsAndCopies)
{
    auto backend{std::make_unique<ClipboardBackend>()};
    const auto clipboard{backend->m_clipboard};
    oui::Application application{std::move(backend)};
    oui::PtyOptions options;
    options.command = {"/bin/sh", "-c", "printf 'alpha beta\\ngamma delta ñu\\n'; sleep 30"};
    oui::TerminalView view{application, options};
    view.arrange({2, 1, 40, 5});
    run_until(application, [&view] { return shows(view.screen(), "delta"); });

    view.handle_event(press(2 + 6, 1 + 0));
    view.handle_event(drag(2 + 4, 1 + 1));
    view.handle_event(release(2 + 4, 1 + 1));
    EXPECT_EQ(view.selected_text(), "beta\ngamma");
    EXPECT_EQ(*clipboard, "beta\ngamma") << "copied when let go";

    view.handle_event(press(2 + 8, 1 + 1, 2));
    view.handle_event(release(2 + 8, 1 + 1));
    EXPECT_EQ(*clipboard, "delta");

    view.handle_event(press(2 + 3, 1 + 1, 3));
    view.handle_event(release(2 + 3, 1 + 1));
    EXPECT_EQ(*clipboard, "gamma delta ñu") << "the whole line, without the spaces after it";

    view.handle_event(press(2 + 1, 1 + 0));
    view.handle_event(release(2 + 1, 1 + 0));
    EXPECT_EQ(view.selected_text(), "") << "a click with no drag selects nothing";
}

TEST(OuiTerminalViewTests, ARightClickPastesAndCtrlShiftCCopies)
{
    auto backend{std::make_unique<ClipboardBackend>()};
    const auto clipboard{backend->m_clipboard};
    *clipboard = "pegado";
    oui::Application application{std::move(backend)};
    oui::PtyOptions options;
    options.command = {"cat"};
    oui::TerminalView view{application, options};
    view.arrange({0, 0, 40, 5});
    auto right{press(3, 3)};
    right.right = true;
    view.handle_event(right);
    run_until(application, [&view] { return shows(view.screen(), "pegado"); });
    EXPECT_TRUE(shows(view.screen(), "pegado"));

    view.handle_event(press(0, 0));
    view.handle_event(drag(2, 0));
    *clipboard = "";
    view.handle_event(oui::TextEvent{U'C', true, false, true});
    EXPECT_EQ(*clipboard, "peg");
    view.handle_event(oui::TextEvent{U'x'});
    EXPECT_EQ(view.selected_text(), "") << "typing leaves the selection";
}

TEST(OuiTerminalViewTests, LeavesTheApplicationItsTabKeys)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sh"};
    const oui::TerminalView view{application, options};
    EXPECT_TRUE(view.claims_key(oui::TextEvent{U'w', true}));
    EXPECT_TRUE(view.claims_key(oui::KeyEvent{oui::Key::f5}));
    EXPECT_TRUE(view.claims_key(oui::TextEvent{U'v', true, false, true}));
    EXPECT_FALSE(view.claims_key(oui::KeyEvent{oui::Key::page_down, true}));
    EXPECT_FALSE(view.claims_key(oui::KeyEvent{oui::Key::tab, true}));
    EXPECT_FALSE(view.claims_key(oui::TextEvent{U'p', true, false, true}));
}

} // namespace

namespace
{

// Whether a process runs with exactly these arguments, found in /proc.
[[nodiscard]] bool running(const std::string& arguments)
{
    for(const auto& entry : std::filesystem::directory_iterator{"/proc"})
    {
        std::ifstream file{entry.path() / "cmdline", std::ios::binary};
        std::string line;
        if(file && std::getline(file, line, '\n') && line == arguments)
        {
            return true;
        }
    }
    return false;
}

} // namespace

// Closing the view ends what runs in it too, not only the shell: the program in front, in a process group of its
// own as a shell with jobs puts it, is hung up as a closed terminal hangs it up.
TEST(OuiTerminalViewTests, ClosingTheViewEndsTheProgramRunningInIt)
{
    const std::string sleeper{std::string{"sleep"} + '\0' + "3071" + '\0'};
    {
        oui::Application application{oui::scripted_backend()};
        oui::PtyOptions options;
        options.command = {"/bin/sh"};
        oui::TerminalView view{application, options};
        view.arrange({0, 0, 80, 24});
        type(view, U"sleep 3071");
        run_until(application, [&view] { return view.title() == "sleep"; });
        ASSERT_TRUE(running(sleeper));
    }
    for(int tries{}; tries < 50 && running(sleeper); ++ tries)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    EXPECT_FALSE(running(sleeper));
}
