#include <oui/application.h>
#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/layout.h>
#include <oui/number_field.h>
#include <oui/scripted_backend.h>
#include <oui/slider.h>

#include <gtest/gtest.h>

#include <memory>

using namespace olagarro;

// A program may bind the arrows, Home, End and the page keys, as a video editor moves its playhead with them; a widget
// with the focus that moves within itself with those keys keeps them: an open drop-down's options, a number field's
// steps, a slider. Elsewhere they are the program's.
TEST(OuiClaimedKeysTests, AWidgetThatMovesWithTheArrowsKeepsThemFromTheProgramsShortcuts)
{
    oui::Application application{oui::scripted_backend({}, {80, 25})};
    auto content{oui::column()};
    auto& combo{content->add(oui::combo_box({"one", "two", "three"}))};
    auto& number{content->add(oui::number_field())};
    number.set_range(0, 100);
    number.set_value(5);
    auto& slider{content->add(oui::slider())};
    slider.set_range(0, 10);
    slider.set_value(5);
    auto& button{content->add(oui::button("Go"))};
    application.set_content(std::move(content));
    application.render();
    auto ran{0};
    application.commands().register_command("go", "Go", [&ran] { ++ ran; });
    for(const auto key : {oui::Key::up, oui::Key::down, oui::Key::left, oui::Key::right, oui::Key::home, oui::Key::end,
                          oui::Key::page_up, oui::Key::page_down})
    {
        application.commands().bind("go", oui::KeyShortcut{key});
    }
    const auto press{[&application](oui::Key key) { application.dispatch_event(oui::KeyEvent{key}); }};

    // Its text, where Tab from the drop-down goes, as a click on it does.
    application.focus(combo);
    press(oui::Key::tab);
    ASSERT_NE(application.focused(), &combo);
    press(oui::Key::up);
    press(oui::Key::page_up);
    EXPECT_EQ(number.value(), 16.0);
    EXPECT_EQ(ran, 0);

    application.focus(slider);
    press(oui::Key::right);
    press(oui::Key::home);
    EXPECT_EQ(slider.value(), 0.0);
    EXPECT_EQ(ran, 0);

    // A drop-down keeps them while it is open, and only then.
    application.focus(combo);
    press(oui::Key::enter);
    press(oui::Key::down);
    press(oui::Key::end);
    EXPECT_EQ(combo.selected(), 2u);
    EXPECT_EQ(ran, 0);
    press(oui::Key::enter);
    press(oui::Key::up);
    EXPECT_EQ(combo.selected(), 2u);
    EXPECT_EQ(ran, 1);

    application.focus(button);
    press(oui::Key::down);
    press(oui::Key::page_down);
    EXPECT_EQ(ran, 3);
}
