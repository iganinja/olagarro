#include <oui/application.h>
#include <oui/line_edit.h>
#include <oui/popup_list.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

using namespace olagarro;

namespace
{

using oui::Key;
using oui::KeyEvent;
using oui::MouseEvent;
using oui::TextEvent;

// An application with a field that has the focus, and a popup of three items under its cell (3, 2).
struct Fixture
{
    Fixture()
    {
        auto backend{oui::scripted_backend({}, {40, 12})};
        scripted = backend.get();
        application = std::make_unique<oui::Application>(std::move(backend));
        auto window{oui::window("Test")};
        window->set_border(oui::WindowBorder::none);
        auto field_widget{oui::line_edit()};
        field = field_widget.get();
        window->add(std::move(field_widget));
        application->set_window(std::move(window));
        application->focus(*field);
        auto list{std::make_unique<oui::PopupList>(oui::Point{3, 2})};
        popup = list.get();
        list->set_items({{"alpha", "word"}, {"beta", ""}, {"gamma", "f()"}});
        list->on_chosen([this](std::size_t index) { chosen = index; });
        list->on_cancelled([this] { cancelled = true; });
        application->show_popup(std::move(list), [this] { closed = true; });
    }

    // Sends `event` and draws what follows.
    void send(const oui::Event& event)
    {
        scripted->push(event);
        application->run_frame();
    }

    [[nodiscard]] std::string row(int y) const
    {
        std::string text;
        const auto& frame{*scripted->last_frame()};
        for(int x{}; x < frame.width(); ++ x)
        {
            text += static_cast<char>(frame.cell_at(x, y).glyph < 128 ? frame.cell_at(x, y).glyph : '?');
        }
        return text;
    }

    oui::ScriptedBackend* scripted{};
    std::unique_ptr<oui::Application> application;
    oui::LineEdit* field{};
    oui::PopupList* popup{};
    std::optional<std::size_t> chosen;
    bool cancelled{false};
    bool closed{false};
};

TEST(OuiPopupListTests, OpensUnderItsCellOverEverythingAndKeepsNoFocus)
{
    Fixture fixture;
    fixture.send(TextEvent{U'x'});
    EXPECT_EQ(fixture.field->text(), "x") << "typing goes on in the field";
    EXPECT_EQ(fixture.application->focused(), fixture.field);
    EXPECT_NE(fixture.row(4).find("alpha"), std::string::npos) << fixture.row(4);
    EXPECT_EQ(fixture.row(4).find("alpha"), 3u) << "the labels start in the anchor's column";
    EXPECT_NE(fixture.row(6).find("gamma"), std::string::npos);
}

TEST(OuiPopupListTests, TheArrowsSelectAndEnterOrTabChoose)
{
    Fixture fixture;
    fixture.send(KeyEvent{Key::down});
    fixture.send(KeyEvent{Key::down});
    fixture.send(KeyEvent{Key::down});
    EXPECT_EQ(fixture.popup->selected(), 2u) << "it stops at the last";
    fixture.send(KeyEvent{Key::up});
    fixture.send(KeyEvent{Key::tab});
    EXPECT_EQ(fixture.chosen, 1u);
    EXPECT_EQ(fixture.application->focused(), fixture.field) << "Tab chose, it did not move the focus";
}

TEST(OuiPopupListTests, EscapeCancelsAndAPressOutsideCloses)
{
    Fixture fixture;
    fixture.send(KeyEvent{Key::escape});
    EXPECT_TRUE(fixture.cancelled);
    EXPECT_FALSE(fixture.closed) << "who showed it decides what Escape does";
    fixture.send(MouseEvent{30, 10, true});
    EXPECT_TRUE(fixture.closed);
    EXPECT_EQ(fixture.application->popup(), nullptr);
}

TEST(OuiPopupListTests, AClickChoosesAndWithNothingToListItSaysSo)
{
    Fixture fixture;
    fixture.send(MouseEvent{4, 5, true});
    EXPECT_EQ(fixture.chosen, 1u);
    fixture.popup->set_items({});
    fixture.popup->set_message("Nothing to suggest");
    fixture.send(KeyEvent{Key::enter});
    EXPECT_NE(fixture.row(4).find("Nothing to suggest"), std::string::npos) << fixture.row(4);
    EXPECT_EQ(fixture.chosen, 1u) << "Enter went on, past a list with nothing to choose";
}

TEST(OuiPopupListTests, OpensOverItsCellWhenThereIsNoRoomBelow)
{
    Fixture fixture;
    fixture.popup->set_anchor({3, 10});
    fixture.send(TextEvent{U'y'});
    EXPECT_NE(fixture.row(6).find("alpha"), std::string::npos) << fixture.row(6);
}

} // namespace
