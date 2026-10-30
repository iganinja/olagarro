#include <oui/application.h>
#include <oui/button.h>
#include <oui/cell_buffer.h>
#include <oui/check_button.h>
#include <oui/event.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/menu_bar.h>
#include <oui/scripted_backend.h>
#include <oui/theme.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

using namespace olagarro;

namespace
{

// A window with a field and two buttons, Save and Quit, each counting its presses.
struct Form
{
    Form()
    {
        auto made{oui::scripted_backend({}, {40, 12})};
        backend = made.get();
        application = std::make_unique<oui::Application>(std::move(made));
        auto window{oui::window("Form")};
        auto content{oui::column()};
        field = &content->add(oui::line_edit());
        save = &content->add(oui::button("&Save"));
        quit = &content->add(oui::button("&Quit"));
        static_cast<void>(save->on_clicked([this] { ++ saved; }));
        static_cast<void>(quit->on_clicked([this] { ++ quitted; }));
        column = content.get();
        window->add(std::move(content));
        application->set_window(std::move(window));
        application->render();
    }

    void click(const oui::Widget& widget)
    {
        const auto at{widget.bounds()};
        application->dispatch_event(oui::MouseEvent{at.x + 1, at.y, true});
        oui::MouseEvent release{at.x + 1, at.y, false};
        release.released = true;
        application->dispatch_event(release);
    }

    oui::ScriptedBackend* backend{};
    std::unique_ptr<oui::Application> application;
    oui::LineEdit* field{};
    oui::Button* save{};
    oui::Button* quit{};
    oui::Column* column{};
    int saved{};
    int quitted{};
};

TEST(OuiEnabledTests, ADisabledButtonTakesNoClickNoLetterAndNoFocus)
{
    Form form;
    form.save->set_enabled(false);
    form.application->render();
    form.click(*form.save);
    form.application->dispatch_event(oui::TextEvent{U's', false, true});
    EXPECT_EQ(form.saved, 0);
    // Tab goes from the field past Save to Quit.
    form.application->focus(*form.field);
    form.application->dispatch_event(oui::KeyEvent{oui::Key::tab});
    EXPECT_TRUE(form.quit->is_focused());

    form.save->set_enabled(true);
    form.application->render();
    form.click(*form.save);
    EXPECT_EQ(form.saved, 1) << "enabled again, it works again";
}

TEST(OuiEnabledTests, WhatADisabledContainerHoldsIsDisabledToo)
{
    Form form;
    form.column->set_enabled(false);
    EXPECT_FALSE(form.quit->is_enabled());
    form.application->render();
    form.click(*form.quit);
    EXPECT_EQ(form.quitted, 0);
    form.column->set_enabled(true);
    EXPECT_TRUE(form.quit->is_enabled());
}

TEST(OuiEnabledTests, ADisabledButtonShowsItsTextInTheDisabledColour)
{
    Form form;
    form.save->set_enabled(false);
    form.application->render();
    ASSERT_TRUE(form.backend->last_frame());
    const auto* const frame{&*form.backend->last_frame()};
    const auto at{form.save->bounds()};
    auto found{false};
    for(int x{at.x}; x < at.x + at.width; ++ x)
    {
        if(frame->cell_at(x, at.y).glyph == U'S')
        {
            found = true;
            EXPECT_EQ(frame->cell_at(x, at.y).foreground, form.application->theme().disabled_foreground);
        }
    }
    EXPECT_TRUE(found);
}

TEST(OuiKeyHandlerTests, AWidgetsKeyHandlerHearsFirstAndCanKeepAKey)
{
    // As a program adds keys of its own to a field: x is the program's, the rest the field's.
    Form form;
    std::vector<oui::Event> heard;
    form.field->set_key_handler(
        [&heard](const oui::Event& event)
        {
            heard.push_back(event);
            const auto* const text{std::get_if<oui::TextEvent>(&event)};
            return text != nullptr && text->codepoint == U'x';
        });
    form.application->focus(*form.field);
    form.application->dispatch_event(oui::TextEvent{U'a'});
    form.application->dispatch_event(oui::TextEvent{U'x'});
    form.application->dispatch_event(oui::TextEvent{U'b'});
    EXPECT_EQ(form.field->text(), "ab");
    EXPECT_EQ(heard.size(), 3u);
}

TEST(OuiFocusHandlerTests, AWidgetsFocusHandlerHearsItTakeAndLoseTheFocus)
{
    // As a program ends what was typed in a field, however the focus leaves it: by Tab, or a click elsewhere.
    Form form;
    std::vector<bool> heard;
    form.field->set_focus_handler([&heard](bool focused) { heard.push_back(focused); });
    ASSERT_TRUE(form.field->is_focused()) << "the window's first field";
    form.application->focus(*form.field);
    form.click(*form.field);
    EXPECT_TRUE(heard.empty()) << "focused again, and clicked in, it neither lost the focus nor took it";
    form.application->dispatch_event(oui::KeyEvent{oui::Key::tab});
    EXPECT_EQ(heard, (std::vector<bool>{false}));
    form.click(*form.field);
    form.click(*form.quit);
    EXPECT_EQ(heard, (std::vector<bool>{false, true, false}));
}

TEST(OuiMenuBarTests, AMenuBarWithNoMenusTakesNoRoom)
{
    oui::CommandRegistry commands;
    auto bar{oui::menu_bar(commands)};
    EXPECT_EQ(bar->measure({80, 25}).height, 0);
    bar->add_menu("&File", {});
    EXPECT_EQ(bar->measure({80, 25}).height, 1);
}

} // namespace
