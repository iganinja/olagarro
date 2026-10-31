#include <oui/event.h>
#include <oui/slider.h>

#include <gtest/gtest.h>

#include <vector>

using namespace olagarro;

namespace
{

oui::MouseEvent press(double x, double y)
{
    return oui::MouseEvent{x, y, true};
}

} // namespace

TEST(OuiSliderTests, AValueIsKeptInItsRangeOnAStep)
{
    oui::Slider slider;
    slider.set_range(1, 100);
    slider.set_step(1);
    slider.set_value(250);
    EXPECT_EQ(slider.value(), 100);
    slider.set_value(-3);
    EXPECT_EQ(slider.value(), 1);
    slider.set_value(41.6);
    EXPECT_EQ(slider.value(), 42);
    slider.set_step(0.5);
    slider.set_value(10.3);
    EXPECT_DOUBLE_EQ(slider.value(), 10.5);
}

TEST(OuiSliderTests, KeysAndTheWheelMoveItAStepAndHomeAndEndToItsEnds)
{
    oui::Slider slider;
    slider.set_range(0, 10);
    slider.set_value(5);
    std::vector<double> changes;
    static_cast<void>(slider.on_changed([&changes](double value) { changes.push_back(value); }));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::right}));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::left}));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::left}));
    oui::MouseEvent wheel{0, 0, false, 1};
    EXPECT_TRUE(slider.handle_event(wheel));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::end}));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::end}));
    EXPECT_TRUE(slider.handle_event(oui::KeyEvent{oui::Key::home}));
    EXPECT_EQ(changes, (std::vector<double>{6, 5, 4, 5, 10, 0})) << "only changes are told";
}

TEST(OuiSliderTests, AClickOrADragPutsTheThumbUnderThePointer)
{
    oui::Slider slider;
    slider.set_range(0, 100);
    slider.arrange({10, 2, 11, 1});
    EXPECT_TRUE(slider.handle_event(press(10, 2)));
    EXPECT_EQ(slider.value(), 0);
    EXPECT_TRUE(slider.handle_event(press(20, 2)));
    EXPECT_EQ(slider.value(), 100);
    oui::MouseEvent drag{15, 2, false};
    drag.dragging = true;
    EXPECT_TRUE(slider.handle_event(drag));
    EXPECT_EQ(slider.value(), 50);
}
