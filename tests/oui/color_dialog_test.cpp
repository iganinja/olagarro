#include <oui/application.h>
#include <oui/backend.h>
#include <oui/color_dialog.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// A step makes its event when it is asked for, from the application as it is then, so it can aim at what the dialog
// shows.
using Step = std::function<oui::Event(oui::Application& application)>;

// Hands out one event per frame, after the first frame, then keeps quiet.
class StepBackend final : public oui::Backend
{
  public:
    explicit StepBackend(std::vector<Step> steps) : m_steps{std::move(steps)} {}

    void set_application(oui::Application& application) { m_application = &application; }

    [[nodiscard]] oui::Size size() const override { return {80, 25}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_application == nullptr || m_frames <= m_next || m_next >= m_steps.size())
        {
            return std::nullopt;
        }
        return m_steps.at(m_next++)(*m_application);
    }

    void present(const oui::CellBuffer&) override { ++ m_frames; }

  private:
    std::vector<Step> m_steps;
    oui::Application* m_application{};
    std::size_t m_next{};
    std::size_t m_frames{};
};

void add_key(std::vector<Step>& steps, oui::Key key, int times = 1, bool shift = false)
{
    for(int index{}; index < times; ++ index)
    {
        steps.emplace_back([key, shift](oui::Application&) { return oui::KeyEvent{key, false, false, shift}; });
    }
}

void add_alt(std::vector<Step>& steps, char32_t letter)
{
    steps.emplace_back([letter](oui::Application&) { return oui::TextEvent{letter, false, true}; });
}

void add_text(std::vector<Step>& steps, std::u32string_view text)
{
    for(const auto character : text)
    {
        steps.emplace_back([character](oui::Application&) { return oui::TextEvent{character}; });
    }
}

// Empties the field focused, from its end.
void add_clearing(std::vector<Step>& steps)
{
    add_key(steps, oui::Key::end);
    add_key(steps, oui::Key::backspace, 8);
}

// What the dialog answered and every colour it told on the way.
struct Asked
{
    std::optional<oui::Color> answer;
    std::vector<oui::Color> heard;
};

Asked ask(oui::Color start, std::vector<Step> steps)
{
    auto backend{std::make_unique<StepBackend>(std::move(steps))};
    auto& stepping{*backend};
    oui::Application application{std::move(backend)};
    stepping.set_application(application);
    application.set_content(oui::window("Background"));
    Asked asked;
    asked.answer =
        oui::ask_color(application, "Colour", start, [&asked](oui::Color color) { asked.heard.push_back(color); });
    return asked;
}

} // namespace

TEST(OuiColorDialogTests, TheColourTypedInHexadecimalIsTheOneAccepted)
{
    std::vector<Step> steps;
    add_alt(steps, U'x');
    add_clearing(steps);
    add_text(steps, U"#336699");
    add_key(steps, oui::Key::enter);
    const auto asked{ask({255, 0, 0}, std::move(steps))};
    EXPECT_EQ(asked.answer, (oui::Color{0x33, 0x66, 0x99}));
    ASSERT_FALSE(asked.heard.empty());
    EXPECT_EQ(asked.heard.back(), (oui::Color{0x33, 0x66, 0x99}));
}

TEST(OuiColorDialogTests, AChannelTypedChangesOnlyThatChannel)
{
    std::vector<Step> steps;
    add_alt(steps, U'g');
    add_clearing(steps);
    add_text(steps, U"128");
    add_key(steps, oui::Key::enter);
    EXPECT_EQ(ask({10, 0, 200}, std::move(steps)).answer, (oui::Color{10, 128, 200}));
}

TEST(OuiColorDialogTests, TheValueTypedKeepsTheHueAndSaturation)
{
    std::vector<Step> steps;
    add_alt(steps, U'v');
    add_clearing(steps);
    add_text(steps, U"50");
    add_key(steps, oui::Key::enter);
    EXPECT_EQ(ask({255, 0, 0}, std::move(steps)).answer, (oui::Color{128, 0, 0}));
}

TEST(OuiColorDialogTests, TheArrowsPickInTheSquareWhichStartsFocused)
{
    // From pure red, at the square's top right: half the value down, half the saturation left.
    std::vector<Step> steps;
    add_key(steps, oui::Key::down, 5, true);
    add_key(steps, oui::Key::left, 5, true);
    add_key(steps, oui::Key::enter);
    const auto asked{ask({255, 0, 0}, std::move(steps))};
    EXPECT_EQ(asked.answer, (oui::Color{128, 64, 64}));
    EXPECT_EQ(asked.heard.front(), (oui::Color{230, 0, 0}));
}

TEST(OuiColorDialogTests, AWholePercentOfValueIsTheByteItRoundsTo)
{
    // From black, a tenth of the value up: 25.5 of 255, which is 26, though 1 - 0.9 falls a hair short of 0.1.
    std::vector<Step> steps;
    add_key(steps, oui::Key::up, 1, true);
    add_key(steps, oui::Key::enter);
    EXPECT_EQ(ask({0, 0, 0}, std::move(steps)).answer, (oui::Color{26, 26, 26}));
}

TEST(OuiColorDialogTests, AClickInTheSquarePicksTheColourThere)
{
    std::vector<Step> steps;
    // The square's top left corner: no saturation, the whole value.
    steps.emplace_back(
        [](oui::Application& application)
        {
            const auto& square{application.focused()->bounds()};
            return oui::MouseEvent{square.x, square.y, true};
        });
    steps.emplace_back(
        [](oui::Application& application)
        {
            const auto& square{application.focused()->bounds()};
            oui::MouseEvent release{square.x, square.y, false};
            release.released = true;
            return release;
        });
    add_key(steps, oui::Key::enter);
    EXPECT_EQ(ask({0, 0, 255}, std::move(steps)).answer, (oui::Color{255, 255, 255}));
}

TEST(OuiColorDialogTests, TheHueBarTurnsTheColourAndAGreyKeepsItsHue)
{
    // Up the bar by a third of the way, 120 steps of a degree: from red to green. Then the value to nothing and back:
    // the hue is still green's.
    std::vector<Step> steps;
    add_key(steps, oui::Key::tab);
    add_key(steps, oui::Key::up, 12, true);
    add_alt(steps, U'v');
    add_clearing(steps);
    add_text(steps, U"0");
    add_clearing(steps);
    add_text(steps, U"100");
    add_key(steps, oui::Key::enter);
    EXPECT_EQ(ask({255, 0, 0}, std::move(steps)).answer, (oui::Color{0, 255, 0}));
}

TEST(OuiColorDialogTests, CancellingAnswersNothing)
{
    std::vector<Step> steps;
    add_key(steps, oui::Key::down, 3, true);
    add_key(steps, oui::Key::escape);
    const auto asked{ask({255, 0, 0}, std::move(steps))};
    EXPECT_FALSE(asked.answer);
    EXPECT_FALSE(asked.heard.empty());
}
