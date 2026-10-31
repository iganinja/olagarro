#include <oui/application.h>
#include <oui/event.h>
#include <oui/layout.h>
#include <oui/scripted_backend.h>
#include <oui/text_metrics.h>
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
        return {static_cast<double>(m_width), constraints.max_height};
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

oui::DropEvent moving(double x, double y)
{
    return oui::DropEvent{oui::DropEvent::Phase::moving, x, y};
}

oui::DropEvent dropped(double x, double y, std::vector<std::string> paths)
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

namespace
{

// A widget of a fixed width that starts a drag of `payload` when the mouse pressed on it moves, and keeps the mouse
// events it hears.
class DragSource final : public oui::Widget
{
  public:
    DragSource(oui::Application& application, oui::DragPayload payload)
        : m_application{application}, m_payload{std::move(payload)}
    {
    }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return {10, constraints.max_height};
    }
    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr)
        {
            return false;
        }
        heard.push_back(*mouse);
        if(mouse->dragging && !m_application.is_dragging())
        {
            m_application.begin_drag(m_payload);
        }
        return mouse->pressed || mouse->dragging || mouse->released;
    }

    std::vector<oui::MouseEvent> heard;

  private:
    oui::Application& m_application;
    oui::DragPayload m_payload;
};

// A source ten cells wide on the left and a recorder that takes drops on the right, twenty wide.
struct DragScreen
{
    DragScreen()
    {
        application = std::make_unique<oui::Application>(oui::scripted_backend({}, {40, 12}));
        auto window{oui::window("Drags")};
        window->set_border(oui::WindowBorder::none);
        auto& sides{oui::row(*window)};
        auto made_source{std::make_unique<DragSource>(
            *application, oui::DragPayload{{"/home/someone/beach.png", "/home/someone/sea.png"}, "2 files"})};
        source = made_source.get();
        sides.add(std::move(made_source));
        auto made_target{std::make_unique<DropRecorder>(20, true)};
        target = made_target.get();
        sides.add(std::move(made_target));
        application->set_window(std::move(window));
        application->render();
    }

    void press(double x, double y) const { application->dispatch_event(oui::MouseEvent{x, y, true}); }
    void drag(double x, double y) const
    {
        oui::MouseEvent move{x, y, false};
        move.dragging = true;
        application->dispatch_event(move);
    }
    void release(double x, double y) const
    {
        oui::MouseEvent up{x, y, false};
        up.released = true;
        application->dispatch_event(up);
    }

    std::unique_ptr<oui::Application> application;
    DragSource* source{};
    DropRecorder* target{};
};

} // namespace

TEST(OuiDropTests, AWidgetStartsADragThatAnotherHearsMoveAndDropWithItsPaths)
{
    DragScreen screen;
    screen.press(3, 3);
    screen.drag(4, 3);
    ASSERT_TRUE(screen.application->is_dragging());
    ASSERT_TRUE(oui::drag_image().has_value());
    EXPECT_EQ(oui::drag_image()->text, "2 files");

    screen.drag(15, 4);
    screen.drag(16, 4);
    ASSERT_EQ(screen.target->heard.size(), 2u);
    EXPECT_EQ(screen.target->heard[0].phase, oui::DropEvent::Phase::moving);
    EXPECT_EQ(screen.target->heard[1].x, 16);
    EXPECT_TRUE(screen.target->heard[1].paths.empty());
    EXPECT_EQ(oui::drag_image()->position.x, 16);

    const auto heard_by_source{screen.source->heard.size()};
    screen.release(17, 5);
    ASSERT_EQ(screen.target->heard.size(), 3u);
    const auto& drop{screen.target->heard[2]};
    EXPECT_EQ(drop.phase, oui::DropEvent::Phase::dropped);
    EXPECT_EQ(drop.x, 17);
    EXPECT_EQ(drop.y, 5);
    EXPECT_EQ(drop.paths, (std::vector<std::string>{"/home/someone/beach.png", "/home/someone/sea.png"}));
    // The moves and the release went to the drag, not to the widget that started it.
    EXPECT_EQ(screen.source->heard.size(), heard_by_source);
    EXPECT_FALSE(screen.application->is_dragging());
    EXPECT_FALSE(oui::drag_image().has_value());
}

TEST(OuiDropTests, EscapeCancelsADragAndTheWidgetUnderItHearsItLeave)
{
    DragScreen screen;
    screen.press(3, 3);
    screen.drag(4, 3);
    screen.drag(15, 4);
    ASSERT_EQ(screen.target->heard.size(), 1u);

    screen.application->dispatch_event(oui::KeyEvent{oui::Key::escape});
    EXPECT_FALSE(screen.application->is_dragging());
    EXPECT_FALSE(oui::drag_image().has_value());
    ASSERT_EQ(screen.target->heard.size(), 2u);
    EXPECT_EQ(screen.target->heard[1].phase, oui::DropEvent::Phase::left);

    // The rest of that click is nobody's: the release drops nothing.
    const auto heard_by_source{screen.source->heard.size()};
    screen.drag(16, 4);
    screen.release(16, 4);
    EXPECT_EQ(screen.target->heard.size(), 2u);
    EXPECT_EQ(screen.source->heard.size(), heard_by_source);
}

TEST(OuiDropTests, ADragLetGoOverNoWidgetDropsNothingAndTheLastHearsItLeave)
{
    DragScreen screen;
    screen.press(3, 3);
    screen.drag(4, 3);
    screen.drag(15, 4);
    screen.release(35, 4);
    ASSERT_EQ(screen.target->heard.size(), 2u);
    EXPECT_EQ(screen.target->heard[1].phase, oui::DropEvent::Phase::left);
    EXPECT_FALSE(screen.application->is_dragging());
}

TEST(OuiDropTests, ADropAWidgetDoesNotTakeGoesToWhatHoldsIt)
{
    Screen screen{{}, false};
    screen.application->dispatch_event(dropped(15, 3, {"/tmp/a.png"}));
    // It heard it and let it go on; nothing round it takes drops, so that is all.
    ASSERT_EQ(screen.right->heard.size(), 1u);
    EXPECT_TRUE(screen.left->heard.empty());
}
