#include <oui/application.h>
#include <oui/event.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// A widget filling the window that keeps every mouse event it hears, and takes the raw pointer when told to.
class Recorder final : public oui::Widget
{
  public:
    explicit Recorder(bool raw) : m_raw{raw} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return {constraints.max_width, constraints.max_height};
    }
    bool handle_event(const oui::Event& event) override
    {
        if(const auto* const mouse{std::get_if<oui::MouseEvent>(&event)})
        {
            heard.push_back(*mouse);
            return true;
        }
        return false;
    }
    [[nodiscard]] bool takes_raw_pointer() const override { return m_raw; }

    std::vector<oui::MouseEvent> heard;

  private:
    bool m_raw;
};

struct Screen
{
    explicit Screen(bool raw)
    {
        application = std::make_unique<oui::Application>(oui::scripted_backend({}, {40, 12}));
        auto window{oui::window("Raw")};
        window->set_border(oui::WindowBorder::none);
        auto made{std::make_unique<Recorder>(raw)};
        recorder = made.get();
        window->add(std::move(made));
        application->set_window(std::move(window));
        application->render();
    }

    void move_to(double x, double y, oui::Point pixel)
    {
        oui::MouseEvent mouse{x, y, false};
        mouse.hover = true;
        mouse.pixel = pixel;
        application->dispatch_event(mouse);
    }

    std::unique_ptr<oui::Application> application;
    Recorder* recorder{};
};

oui::MouseEvent middle_press(double x, double y)
{
    oui::MouseEvent mouse{x, y, true};
    mouse.middle = true;
    return mouse;
}

} // namespace

TEST(OuiRawPointerTests, AWidgetHearsThePointerMoveFromCellToCell)
{
    Screen screen{false};
    screen.move_to(5, 5, {40, 80});
    screen.move_to(5, 5, {41, 80});
    screen.move_to(5, 5, {42, 81});
    screen.move_to(6, 5, {48, 81});
    ASSERT_EQ(screen.recorder->heard.size(), 2u);
    EXPECT_EQ(screen.recorder->heard[1].x, 6);
}

TEST(OuiRawPointerTests, AWidgetTakingTheRawPointerHearsItMoveFromPixelToPixel)
{
    Screen screen{true};
    screen.move_to(5, 5, {40, 80});
    screen.move_to(5, 5, {41, 80});
    screen.move_to(5, 5, {42, 81});
    screen.move_to(6, 5, {48, 81});
    ASSERT_EQ(screen.recorder->heard.size(), 4u);
    ASSERT_TRUE(screen.recorder->heard[2].pixel.has_value());
    EXPECT_EQ(screen.recorder->heard[2].pixel->x, 42);
    EXPECT_EQ(screen.recorder->heard[2].pixel->y, 81);
}

TEST(OuiRawPointerTests, OnlyAWidgetTakingTheRawPointerHearsTheMiddleButton)
{
    Screen plain{false};
    plain.application->dispatch_event(middle_press(3, 3));
    EXPECT_TRUE(plain.recorder->heard.empty()) << "a middle click must not reach a widget as a left click";

    Screen raw{true};
    raw.application->dispatch_event(middle_press(3, 3));
    ASSERT_EQ(raw.recorder->heard.size(), 1u);
    EXPECT_TRUE(raw.recorder->heard[0].middle);
}
