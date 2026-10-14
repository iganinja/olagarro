#include <oui/event.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>

#include <gtest/gtest.h>

#include <vector>

using namespace olagarro;

TEST(OuiNumberFieldTests, ShowsItsValueWithTheDecimalsAsked)
{
    oui::NumberField field;
    field.set_range(0, 1000);
    field.set_value(640);
    EXPECT_EQ(field.text_field().text(), "640");
    field.set_decimals(2);
    field.set_step(0.25);
    field.set_value(1.5);
    EXPECT_EQ(field.text_field().text(), "1.50");
}

TEST(OuiNumberFieldTests, WhatIsTypedIsTakenOnEnterKeptInRange)
{
    oui::NumberField field;
    field.set_range(1, 100);
    field.set_value(10);
    std::vector<double> changes;
    static_cast<void>(field.on_changed([&changes](double value) { changes.push_back(value); }));
    // Enter goes from the field, where nothing hears it, up to the number field, which takes what was typed and lets
    // it go on, as to the window's default button.
    const auto enter{[&field]
                     {
                         EXPECT_FALSE(field.text_field().handle_event(oui::KeyEvent{oui::Key::enter}));
                         EXPECT_FALSE(field.handle_event(oui::KeyEvent{oui::Key::enter}));
                     }};
    field.text_field().set_text("250");
    enter();
    EXPECT_EQ(field.value(), 100);
    EXPECT_EQ(field.text_field().text(), "100");
    field.text_field().set_text("abc");
    enter();
    EXPECT_EQ(field.value(), 100) << "what is not a number changes nothing";
    EXPECT_EQ(field.text_field().text(), "100") << "and the value shows again";
    EXPECT_EQ(changes, (std::vector<double>{100}));
}

TEST(OuiNumberFieldTests, UpAndDownTheWheelAndTheArrowsStepIt)
{
    oui::NumberField field;
    field.set_range(0, 10);
    field.set_value(5);
    field.arrange({0, 0, 8, 1});
    EXPECT_TRUE(field.text_field().key_handler_keeps(oui::KeyEvent{oui::Key::up}));
    EXPECT_EQ(field.value(), 6);
    EXPECT_TRUE(field.text_field().key_handler_keeps(oui::KeyEvent{oui::Key::down}));
    EXPECT_EQ(field.value(), 5);
    oui::MouseEvent wheel{2, 0, false, -1};
    EXPECT_TRUE(field.handle_event(wheel));
    EXPECT_EQ(field.value(), 4);
    // The arrows are the last two cells: down, then up.
    EXPECT_TRUE(field.handle_event(oui::MouseEvent{7, 0, true}));
    EXPECT_EQ(field.value(), 5);
    EXPECT_TRUE(field.handle_event(oui::MouseEvent{6, 0, true}));
    EXPECT_EQ(field.value(), 4);
    EXPECT_EQ(field.text_field().text(), "4");
}

TEST(OuiNumberFieldTests, ANumberTypedIsTheValueAtOnceAndShowsKeptWhenCommitted)
{
    oui::NumberField field;
    field.set_range(0, 100);
    field.text_field().set_text("42");
    EXPECT_EQ(field.value(), 42) << "leaving the field without Enter loses nothing";
    field.text_field().set_text("420");
    EXPECT_EQ(field.value(), 100);
    EXPECT_EQ(field.text_field().text(), "420") << "not rewritten while being typed";
    field.commit();
    EXPECT_EQ(field.text_field().text(), "100");
}
