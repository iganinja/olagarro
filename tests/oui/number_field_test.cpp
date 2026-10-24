#include <oui/application.h>
#include <oui/event.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <memory>
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

namespace
{

// A draggable number field from 0 to 100 at 50, alone in a window of the scripted backend, and what it told.
struct DraggedField
{
    oui::ScriptedBackend* backend{};
    std::unique_ptr<oui::Application> application;
    oui::NumberField* field{};
    std::vector<double> changes;
    int started{};
    int finished{};

    DraggedField()
    {
        auto made{oui::scripted_backend({}, {20, 3})};
        backend = made.get();
        application = std::make_unique<oui::Application>(std::move(made));
        auto column{oui::column()};
        field = &oui::number_field(*column);
        field->set_range(0, 100);
        field->set_value(50);
        field->set_draggable(true);
        static_cast<void>(field->on_changed([this](double value) { changes.push_back(value); }));
        static_cast<void>(field->on_drag_started([this] { ++ started; }));
        static_cast<void>(field->on_drag_finished([this] { ++ finished; }));
        application->set_content(std::move(column));
        application->run_frame();
    }

    void play(std::vector<oui::Event> events)
    {
        for(const auto& event : events)
        {
            backend->push(event);
        }
        application->run_frame();
    }
};

[[nodiscard]] oui::MouseEvent press(int x, int clicks = 1)
{
    oui::MouseEvent event{x, 0, true};
    event.clicks = clicks;
    return event;
}

[[nodiscard]] oui::MouseEvent drag(int x, bool shift = false, bool control = false)
{
    oui::MouseEvent event{x, 0, false, 0, true};
    event.shift = shift;
    event.control = control;
    return event;
}

[[nodiscard]] oui::MouseEvent release(int x)
{
    return oui::MouseEvent{x, 0, false, 0, false, true};
}

} // namespace

TEST(OuiNumberFieldTests, DraggedAlongItTakesAStepACellATenthWithShiftAndTenWithCtrl)
{
    DraggedField dragged;
    dragged.play({press(2), drag(5)});
    EXPECT_EQ(dragged.field->value(), 53);
    EXPECT_EQ(dragged.field->text_field().text(), "53") << "shown while it is dragged, focused as it is";
    EXPECT_EQ(dragged.started, 1);
    EXPECT_EQ(dragged.finished, 0);
    dragged.play({drag(7, true)});
    EXPECT_NEAR(dragged.field->value(), 53.2, 1e-9) << "Shift goes between the steps";
    dragged.play({drag(6, false, true)});
    EXPECT_NEAR(dragged.field->value(), 43.2, 1e-9);
    dragged.play({drag(0, false, true), drag(1)});
    EXPECT_NEAR(dragged.field->value(), 1, 1e-9) << "held at the minimum, and from there at once on the way back";
    dragged.play({release(1)});
    EXPECT_EQ(dragged.started, 1);
    EXPECT_EQ(dragged.finished, 1);
    EXPECT_EQ(dragged.changes.size(), 5u);
}

TEST(OuiNumberFieldTests, AClickWithoutMovingTypesInItAndADoubleClickSelectsWhatItShows)
{
    DraggedField dragged;
    dragged.play({press(1), release(1)});
    EXPECT_EQ(dragged.field->value(), 50);
    EXPECT_EQ(dragged.started, 0) << "no drag without a move";
    EXPECT_EQ(dragged.finished, 0);
    EXPECT_TRUE(dragged.field->text_field().is_focused());
    dragged.play({oui::TextEvent{U'7'}});
    EXPECT_EQ(dragged.field->text_field().text(), "570") << "the cursor where it was clicked, after the 5";

    dragged.field->set_value(50);
    dragged.field->commit();
    // On another cell, so as not to count with the click before.
    dragged.play({press(0), release(0), press(0), release(0), oui::TextEvent{U'8'}});
    EXPECT_EQ(dragged.field->text_field().text(), "8") << "typed over the whole number";
    EXPECT_EQ(dragged.field->value(), 8);
    EXPECT_EQ(dragged.started, 0);
}

TEST(OuiNumberFieldTests, NotDraggableADragSelectsItsText)
{
    DraggedField dragged;
    dragged.field->set_draggable(false);
    dragged.play({press(0), drag(2), release(2), oui::TextEvent{U'9'}});
    EXPECT_EQ(dragged.field->value(), 9) << "the 50 selected and typed over";
    EXPECT_EQ(dragged.started, 0);
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
