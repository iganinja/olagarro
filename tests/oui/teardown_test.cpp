// Built with OUI itself under AddressSanitizer: what goes wrong only as memory is freed shows here.
#include <oui/application.h>
#include <oui/scripted_backend.h>
#include <oui/terminal_view.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

using namespace olagarro;

// The application takes its widgets down while the rest of it is whole. A terminal's thread tells the application
// its program ended as the terminal goes; it did so into the application's queue, already gone, and OUI Code broke
// with "corrupted size vs. prev_size" on quitting with a terminal open.
TEST(OuiTeardownTests, ATerminalClosingWithTheApplicationTellsAWholeApplication)
{
    oui::Application application{oui::scripted_backend()};
    oui::PtyOptions options;
    options.command = {"/bin/sh"};
    application.set_content(std::make_unique<oui::TerminalView>(application, options));
    for(int frame{}; frame < 20; ++ frame)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    // Something still waiting to run when it goes, as there was in OUI Code: the queue had its memory then.
    application.post([] {});
}
