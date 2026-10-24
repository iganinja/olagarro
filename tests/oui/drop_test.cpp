#include <oui/application.h>
#include <oui/event.h>
#include <oui/layout.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// A widget of a fixed width that keeps every drop it hears, and takes drops only when told to.
class DropRecorder final : public oui::Widget
{
  public:
    DropRecorder(int width, bool takes) : m_width{width}, m_takes{takes} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return {m_width, constraints.max_height};
    }
    bool handle_event(const oui::Event& event) override
    {
        if(const auto* const drop{std::get_if<oui::DropEvent>(&event)})
        {
            heard.push_back(*drop);
            return m_takes;
        }
        return false;
    }

    std::vector<oui::DropEvent> heard;

  private:
    int m_width;
    bool m_takes;
};

// Two widgets side by side, the left one ten cells wide and the right one twenty, and ten cells of nothing after
// them; the right one hears drops but does not take them unless `right_takes`.
struct Screen
{
    explicit Screen(std::vector<oui::Event> events = {}, bool right_takes = true)
    {
        application = std::make_unique<oui::Application>(oui::scripted_backend(std::move(events), {40, 12}));
        auto window{oui::window("Drops")};
        window->set_border(oui::WindowBorder::none);
        auto& sides{oui::row(*window)};
        auto made_left{std::make_unique<DropRecorder>(10, true)};
        left = made_left.get();
        sides.add(std::move(made_left));
        auto made_right{std::make_unique<DropRecorder>(20, right_takes)};
        right = made_right.get();
        sides.add(std::move(made_right));
        application->set_window(std::move(window));
        application->render();
    }

    std::unique_ptr<oui::Application> application;
    DropRecorder* left{};
    DropRecorder* right{};
};

oui::DropEvent moving(int x, int y)
{
    return oui::DropEvent{oui::DropEvent::Phase::moving, x, y};
}

oui::DropEvent dropped(int x, int y, std::vector<std::string> paths)
{
    return oui::DropEvent{oui::DropEvent::Phase::dropped, x, y, std::move(paths)};
}

} // namespace

TEST(OuiDropTests, ADropReachesTheWidgetUnderItWithItsPaths)
{
    Screen screen{{dropped(15, 4, {"/home/someone/beach.mp4", "/home/someone/música.wav"})}};
    screen.application->run_frame();

    EXPECT_TRUE(screen.left->heard.empty());
    ASSERT_EQ(screen.right->heard.size(), 1u);
    const auto& drop{screen.right->heard[0]};
    EXPECT_EQ(drop.phase, oui::DropEvent::Phase::dropped);
    EXPECT_EQ(drop.x, 15);
    EXPECT_EQ(drop.y, 4);
    EXPECT_EQ(drop.paths, (std::vector<std::string>{"/home/someone/beach.mp4", "/home/someone/música.wav"}));
}

TEST(OuiDropTests, AWidgetHearsWhereTheFilesAreWhileTheyAreCarriedOverIt)
{
    Screen screen;
    auto first{moving(12, 3)};
    first.pixel = oui::Point{100, 50};
    screen.application->dispatch_event(first);
    screen.application->dispatch_event(moving(13, 3));
    screen.application->dispatch_event(moving(14, 5));

    EXPECT_TRUE(screen.left->heard.empty());
    ASSERT_EQ(screen.right->heard.size(), 3u);
    for(const auto& drop : screen.right->heard)
    {
        EXPECT_EQ(drop.phase, oui::DropEvent::Phase::moving);
        EXPECT_TRUE(drop.paths.empty());
    }
    ASSERT_TRUE(screen.right->heard[0].pixel.has_value());
    EXPECT_EQ(screen.right->heard[0].pixel->x, 100);
    EXPECT_EQ(screen.right->heard[0].pixel->y, 50);
    EXPECT_EQ(screen.right->heard[2].x, 14);
    EXPECT_EQ(screen.right->heard[2].y, 5);
}

TEST(OuiDropTests, AWidgetTheFilesWentAwayFromHearsThatTheyLeft)
{
    Screen screen;
    screen.application->dispatch_event(moving(15, 3));
    screen.application->dispatch_event(moving(5, 3));
    ASSERT_EQ(screen.right->heard.size(), 2u);
    EXPECT_EQ(screen.right->heard[1].phase, oui::DropEvent::Phase::left);
    ASSERT_EQ(screen.left->heard.size(), 1u);
    EXPECT_EQ(screen.left->heard[0].phase, oui::DropEvent::Phase::moving);

    // Out of the window, as the backend says when the files are carried away from it.
    screen.application->dispatch_event(oui::DropEvent{oui::DropEvent::Phase::left});
    ASSERT_EQ(screen.left->heard.size(), 2u);
    EXPECT_EQ(screen.left->heard[1].phase, oui::DropEvent::Phase::left);
    EXPECT_EQ(screen.right->heard.size(), 2u);
}

TEST(OuiDropTests, AWidgetThatHeardTheFilesMoveHearsThemLeaveWhenTheyAreDroppedElsewhere)
{
    Screen screen;
    screen.application->dispatch_event(moving(15, 3));
    screen.application->dispatch_event(dropped(5, 3, {"/tmp/a.png"}));
    ASSERT_EQ(screen.right->heard.size(), 2u);
    EXPECT_EQ(screen.right->heard[1].phase, oui::DropEvent::Phase::left);
    ASSERT_EQ(screen.left->heard.size(), 1u);
    EXPECT_EQ(screen.left->heard[0].phase, oui::DropEvent::Phase::dropped);

    // Once dropped nobody is waiting for them: the next files start afresh.
    screen.application->dispatch_event(oui::DropEvent{oui::DropEvent::Phase::left});
    EXPECT_EQ(screen.left->heard.size(), 1u);
    EXPECT_EQ(screen.right->heard.size(), 2u);
}

TEST(OuiDropTests, ADropOnNoWidgetIsIgnored)
{
    Screen screen{{moving(35, 3), dropped(35, 3, {"/tmp/a.png"})}};
    screen.application->run_frame();
    EXPECT_TRUE(screen.left->heard.empty());
    EXPECT_TRUE(screen.right->heard.empty());
}

TEST(OuiDropTests, ADropAWidgetDoesNotTakeGoesToWhatHoldsIt)
{
    Screen screen{{}, false};
    screen.application->dispatch_event(dropped(15, 3, {"/tmp/a.png"}));
    // It heard it and let it go on; nothing round it takes drops, so that is all.
    ASSERT_EQ(screen.right->heard.size(), 1u);
    EXPECT_TRUE(screen.left->heard.empty());
}
