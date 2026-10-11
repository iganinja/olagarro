#include "find_bar.h"

#include <oui/application.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/line_edit.h>
#include <oui/dock_layout.h>
#include <oui/scripted_backend.h>
#include <oui/text_edit.h>
#include <oui/translation.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

using namespace olagarro;

namespace
{

// The row of the frame as text, to look for what it shows.
[[nodiscard]] std::u32string row_of(const oui::CellBuffer& frame, int y)
{
    std::u32string text;
    for(int x{}; x < frame.width(); ++ x)
    {
        text += frame.cell_at(x, y).glyph;
    }
    return text;
}

} // namespace

TEST(FindBarTests, ItsOptionsShowOverTheDocumentAboveIt)
{
    // As OUI Code lays it out: the bar under the documents, which are added after it.
    auto backend{oui::scripted_backend({}, {60, 12})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Test")};
    window->set_border(oui::WindowBorder::none);
    auto layout{oui::dock_layout()};
    auto document{oui::text_edit()};
    document->set_text("one two three");
    auto& document_reference{*document};
    auto bar{std::make_unique<FindBar>(application, document_reference)};
    auto& bar_reference{*bar};
    layout->add(oui::Dock::bottom, std::move(bar));
    layout->add(oui::Dock::fill, std::move(document));
    window->add(std::move(layout));
    application.set_window(std::move(window));
    bar_reference.open(document_reference);
    application.render();

    // The options' button, at the end of the bar's row.
    const auto bottom{11};
    const auto row{row_of(*screen->last_frame(), bottom)};
    const auto star{row.find(U'*')};
    ASSERT_NE(star, std::u32string::npos);
    application.dispatch_event(oui::MouseEvent{static_cast<int>(star), bottom, true});
    application.dispatch_event(oui::MouseEvent{static_cast<int>(star), bottom, false, 0, false, true});
    application.render();

    auto shown{false};
    for(int y{}; y < bottom; ++ y)
    {
        shown = shown || row_of(*screen->last_frame(), y).find(U"Whole word") != std::u32string::npos;
    }
    EXPECT_TRUE(shown) << "the options open over the document, not under it";

    // A click elsewhere closes them.
    application.dispatch_event(oui::MouseEvent{1, 1, true});
    application.dispatch_event(oui::MouseEvent{1, 1, false, 0, false, true});
    application.render();
    for(int y{}; y < bottom; ++ y)
    {
        EXPECT_EQ(row_of(*screen->last_frame(), y).find(U"Whole word"), std::u32string::npos);
    }
}

TEST(FindBarTests, CountsTheMatchItFoundAsYouType)
{
    auto backend{oui::scripted_backend({}, {60, 6})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Test")};
    window->set_border(oui::WindowBorder::none);
    auto layout{oui::dock_layout()};
    auto document{oui::text_edit()};
    document->set_text("one two three two");
    auto& document_reference{*document};
    // Made for another document, as OUI Code makes it for the first one, and opened on this one.
    oui::TextEdit first;
    auto bar{std::make_unique<FindBar>(application, first)};
    auto& bar_reference{*bar};
    layout->add(oui::Dock::bottom, std::move(bar));
    layout->add(oui::Dock::fill, std::move(document));
    window->add(std::move(layout));
    application.set_window(std::move(window));
    bar_reference.open(document_reference);
    for(const auto character : std::u32string{U"two"})
    {
        application.dispatch_event(oui::TextEvent{character});
    }
    application.render();
    EXPECT_NE(row_of(*screen->last_frame(), 5).find(U"1 of 2"), std::u32string::npos)
        << "the first match is selected, and the count says so";
}

TEST(FindBarTests, ADocumentMadeWhereAClosedOneWasIsFollowedAsANewOne)
{
    auto backend{oui::scripted_backend({}, {60, 3})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    // The documents live in one place, so the second is made exactly where the first was, as the allocator may do
    // when a tab closes and another opens.
    std::optional<oui::TextEdit> slot;
    slot.emplace();
    slot->set_text("one two");
    auto window{oui::window("Test")};
    window->set_border(oui::WindowBorder::none);
    auto bar{std::make_unique<FindBar>(application, *slot)};
    auto& bar_reference{*bar};
    window->add(std::move(bar));
    application.set_window(std::move(window));
    bar_reference.open(*slot);
    for(const auto character : std::u32string{U"two"})
    {
        application.dispatch_event(oui::TextEvent{character});
    }

    slot.reset();
    slot.emplace();
    slot->set_text("x two y two z two");
    bar_reference.open(*slot);
    bar_reference.next_match();
    application.render();
    EXPECT_NE(row_of(*screen->last_frame(), 0).find(U"1 of 3"), std::u32string::npos)
        << "the bar follows the new document, though it is where the old one was";
}

TEST(FindBarTests, ItsLabelShowsWholeInAnyLanguage)
{
    // Longer than the English, as Spanish is.
    oui::TranslationCatalog catalog{"es", "Español", {}};
    catalog.contexts["FindBar"]["Find:"] = "Buscar:";
    oui::remove_translations();
    oui::install_translation(oui::Translation{catalog});
    auto backend{oui::scripted_backend({}, {60, 3})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    oui::TextEdit document;
    auto window{oui::window("Test")};
    window->set_border(oui::WindowBorder::none);
    auto bar{std::make_unique<FindBar>(application, document)};
    auto& bar_reference{*bar};
    window->add(std::move(bar));
    application.set_window(std::move(window));
    bar_reference.open(document);
    application.render();
    oui::remove_translations();
    EXPECT_EQ(row_of(*screen->last_frame(), 0).rfind(U"Buscar:", 0), 0u);
}

TEST(FindBarTests, ItsOptionsTakeTheFocusAndEscapeGivesItBackToTheField)
{
    auto backend{oui::scripted_backend({}, {60, 12})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    auto window{oui::window("Test")};
    window->set_border(oui::WindowBorder::none);
    auto layout{oui::dock_layout()};
    auto document{oui::text_edit()};
    auto& document_reference{*document};
    auto bar{std::make_unique<FindBar>(application, document_reference)};
    auto& bar_reference{*bar};
    layout->add(oui::Dock::bottom, std::move(bar));
    layout->add(oui::Dock::fill, std::move(document));
    window->add(std::move(layout));
    application.set_window(std::move(window));
    bar_reference.open(document_reference);
    application.render();
    auto* const field{application.focused()};
    ASSERT_NE(dynamic_cast<oui::LineEdit*>(field), nullptr);

    // The options' button, pressed from the keyboard.
    const auto row{row_of(*screen->last_frame(), 11)};
    auto* const star{bar_reference.hit_test(static_cast<int>(row.find(U'*')), 11)};
    ASSERT_NE(dynamic_cast<oui::Button*>(star), nullptr);
    application.focus(*star);
    application.dispatch_event(oui::KeyEvent{oui::Key::enter});
    auto* const first{dynamic_cast<oui::CheckButton*>(application.focused())};
    ASSERT_NE(first, nullptr) << "the focus goes to the first option";
    EXPECT_EQ(first->text(), "Whole word");

    application.dispatch_event(oui::KeyEvent{oui::Key::escape});
    application.render();
    EXPECT_EQ(application.focused(), field) << "Escape closes the options, back to the field";
    EXPECT_TRUE(bar_reference.is_visible()) << "and only them";
    for(int y{}; y < 11; ++ y)
    {
        EXPECT_EQ(row_of(*screen->last_frame(), y).find(U"Whole word"), std::u32string::npos);
    }
}
