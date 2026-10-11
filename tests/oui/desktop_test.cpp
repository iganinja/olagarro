#include <oui/application.h>
#include <oui/desktop.h>
#include <oui/graphics/desktop.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

using namespace olagarro;

namespace
{

template <typename Done> void run_until(oui::Application& application, Done done)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(!done() && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

[[nodiscard]] std::filesystem::path test_socket()
{
    return std::filesystem::temp_directory_path() / ("oui-desktop-test-" + std::to_string(::getpid()));
}

// A program on the desktop says who it is, is given its size and the keys, draws a frame the desktop gets whole,
// hears its input, and is gone from the desktop when it ends.
TEST(OuiDesktopTests, AProgramShowsOnTheDesktopAndHearsItsInput)
{
    oui::Application application{oui::scripted_backend()};
    oui::DesktopServer server{application, test_socket()};
    std::optional<int> connected;
    std::optional<int> disconnected;
    auto frames{0};
    server.on_connected(
        [&server, &connected](int id)
        {
            connected = id;
            server.configure(id, 30, 8, true);
        });
    server.on_changed([&frames](int) { ++ frames; });
    server.on_disconnected([&disconnected](int id) { disconnected = id; });

    std::vector<oui::Event> heard;
    std::atomic<bool> key_sent{false};
    std::thread program{
        [&heard, &key_sent]
        {
            oui::DesktopClientOptions options;
            options.title = "probe";
            options.socket = test_socket();
            auto backend{oui::desktop_backend(options, oui::classic_style())};
            EXPECT_EQ(backend->size().width, 30);
            EXPECT_EQ(backend->size().height, 8);
            backend->render([](oui::Painter& painter) { painter.fill_rect({0, 0, 30, 8}, {200, 10, 20}); });
            const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
            while(std::chrono::steady_clock::now() < deadline)
            {
                backend->wait_for_input(std::chrono::steady_clock::now() + std::chrono::milliseconds{20});
                while(const auto event{backend->poll_event()})
                {
                    heard.push_back(*event);
                }
                if(key_sent && !heard.empty() && std::holds_alternative<oui::KeyEvent>(heard.back()))
                {
                    return;
                }
            }
        }};
    run_until(application, [&frames] { return frames > 0; });
    ASSERT_TRUE(connected.has_value());
    const auto* const client{server.client(*connected)};
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->title, "probe");
    ASSERT_FALSE(client->pixels.empty());
    EXPECT_EQ(client->width % 30, 0) << "a whole number of cells across";
    EXPECT_EQ(client->height % 8, 0);
    EXPECT_EQ(client->pixels.front(), 0xC80A14u) << "what the program drew";
    server.send(*connected, oui::MouseEvent{3, 2, true});
    server.send(*connected, oui::KeyEvent{oui::Key::f5, true});
    key_sent = true;
    run_until(application, [&disconnected] { return disconnected.has_value(); });
    program.join();
    EXPECT_EQ(disconnected, connected);
    ASSERT_GE(heard.size(), 2u);
    const auto* const mouse{std::get_if<oui::MouseEvent>(&heard[heard.size() - 2])};
    ASSERT_NE(mouse, nullptr);
    EXPECT_EQ(mouse->x, 3);
    EXPECT_EQ(mouse->y, 2);
    EXPECT_TRUE(mouse->pressed);
    const auto* const key{std::get_if<oui::KeyEvent>(&heard.back())};
    ASSERT_NE(key, nullptr);
    EXPECT_EQ(key->key, oui::Key::f5);
    EXPECT_TRUE(key->control);
}

// What one program copies, the others paste, a program that comes later too; and a program's title reaches the
// desktop when it changes.
TEST(OuiDesktopTests, ProgramsShareTheClipboardAndTellTheirTitles)
{
    oui::Application application{oui::scripted_backend()};
    oui::DesktopServer server{application, test_socket()};
    std::vector<int> ids;
    server.on_connected(
        [&server, &ids](int id)
        {
            ids.push_back(id);
            server.configure(id, 20, 5, false);
        });
    std::vector<std::unique_ptr<oui::Backend>> programs;
    // A program waits for its size, which the desktop gives on its own thread: each starts beside it.
    const auto start{[&application, &programs](const char* title)
                     {
                         std::unique_ptr<oui::Backend> backend;
                         std::thread starting{[&backend, title]
                                              {
                                                  oui::DesktopClientOptions options;
                                                  options.title = title;
                                                  options.socket = test_socket();
                                                  backend = oui::desktop_backend(options, oui::classic_style());
                                              }};
                         run_until(application, [&backend] { return backend != nullptr; });
                         starting.join();
                         programs.push_back(std::move(backend));
                     }};
    // What a program has heard so far.
    const auto heard{[](oui::Backend& program)
                     {
                         while(program.poll_event())
                         {
                         }
                     }};
    start("first");
    start("second");
    ASSERT_EQ(ids.size(), 2u);
    ASSERT_TRUE(programs[0]->keeps_clipboard());
    programs[0]->set_clipboard_text("copied ñ\nwith a second line");
    programs[0]->set_title("renamed");
    run_until(application, [&server, &ids] { return server.client(ids[0])->title == "renamed"; });
    EXPECT_EQ(server.client(ids[0])->title, "renamed");
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(programs[1]->clipboard_text().empty() && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        heard(*programs[1]);
    }
    EXPECT_EQ(programs[1]->clipboard_text(), "copied ñ\nwith a second line");
    start("third");
    heard(*programs[2]);
    EXPECT_EQ(programs[2]->clipboard_text(), "copied ñ\nwith a second line") << "given when it says hello";
}

// On a desktop in a terminal a program is a backend of cells, and the desktop gets the cells it draws.
TEST(OuiDesktopTests, OnADesktopOfCellsAProgramDrawsCells)
{
    oui::Application application{oui::scripted_backend()};
    oui::DesktopServer server{application, test_socket(), oui::DesktopFrames::cells};
    std::optional<int> connected;
    auto frames{0};
    server.on_connected(
        [&server, &connected](int id)
        {
            connected = id;
            server.configure(id, 12, 3, true);
        });
    server.on_changed([&frames](int) { ++ frames; });
    std::unique_ptr<oui::Backend> program;
    std::thread starting{[&program]
                         {
                             oui::DesktopClientOptions options;
                             options.socket = test_socket();
                             program = oui::desktop_backend(options);
                         }};
    run_until(application, [&program] { return program != nullptr; });
    starting.join();
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(dynamic_cast<oui::PixelBackend*>(program.get()), nullptr) << "cells, not pixels";
    EXPECT_EQ(program->size().width, 12);
    EXPECT_EQ(program->size().height, 3);
    program->render([](oui::Painter& painter)
                    { painter.draw_text({1, 2}, "ñandú", {{10, 20, 30}, {40, 50, 60}, oui::text_bold}); });
    run_until(application, [&frames] { return frames > 0; });
    const auto* const client{server.client(*connected)};
    ASSERT_NE(client, nullptr);
    EXPECT_TRUE(client->pixels.empty());
    ASSERT_EQ(client->cells.size(), 36u);
    EXPECT_EQ(client->width, 12);
    EXPECT_EQ(client->height, 3);
    const auto& first{client->cells[2 * 12 + 1]};
    EXPECT_EQ(first.glyph, U'ñ');
    EXPECT_EQ(first.foreground, (oui::Color{10, 20, 30}));
    EXPECT_EQ(first.background, (oui::Color{40, 50, 60}));
    EXPECT_EQ(first.attributes, oui::text_bold);
    EXPECT_EQ(client->cells[2 * 12 + 5].glyph, U'ú');
}

// A program built without the graphics draws only cells, and says so on a desktop of pixels.
TEST(OuiDesktopTests, AProgramOfCellsIsToldWhenTheDesktopShowsPixels)
{
    oui::Application application{oui::scripted_backend()};
    oui::DesktopServer server{application, test_socket()};
    server.on_connected([&server](int id) { server.configure(id, 10, 4, true); });
    std::optional<std::string> refused;
    std::atomic<bool> done{false};
    std::thread starting{[&refused, &done]
                         {
                             oui::DesktopClientOptions options;
                             options.socket = test_socket();
                             try
                             {
                                 static_cast<void>(oui::desktop_backend(options));
                             }
                             catch(const std::system_error& error)
                             {
                                 refused = error.what();
                             }
                             done = true;
                         }};
    run_until(application, [&done] { return done.load(); });
    starting.join();
    ASSERT_TRUE(refused.has_value());
    EXPECT_NE(refused->find("draws only characters"), std::string::npos) << *refused;
}

TEST(OuiDesktopTests, WithoutADesktopAProgramIsToldSo)
{
    oui::DesktopClientOptions options;
    options.socket = test_socket();
    EXPECT_THROW(static_cast<void>(oui::desktop_backend(options)), std::system_error);
}

} // namespace

// A program asks for the pointer it wants over its window, as over a line it can drag; the desktop, which draws the
// pointer, hears it and says so.
TEST(OuiDesktopTests, AProgramTellsThePointerItWants)
{
    oui::Application application{oui::scripted_backend()};
    oui::DesktopServer server{application, test_socket()};
    std::optional<int> id;
    server.on_connected(
        [&server, &id](int connected)
        {
            id = connected;
            server.configure(connected, 20, 5, true);
        });
    std::vector<int> told;
    server.on_pointer([&told](int changed) { told.push_back(changed); });
    std::unique_ptr<oui::Backend> program;
    std::thread starting{[&program]
                         {
                             oui::DesktopClientOptions options;
                             options.socket = test_socket();
                             program = oui::desktop_backend(options, oui::classic_style());
                         }};
    run_until(application, [&program] { return program != nullptr; });
    starting.join();
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(server.client(*id)->pointer, oui::Pointer::arrow);
    program->set_pointer(oui::Pointer::resize_rows);
    run_until(application, [&told] { return !told.empty(); });
    EXPECT_EQ(told, std::vector<int>{*id});
    EXPECT_EQ(server.client(*id)->pointer, oui::Pointer::resize_rows);
}
