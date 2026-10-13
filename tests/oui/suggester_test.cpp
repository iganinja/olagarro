#include <oui/application.h>
#include <oui/label.h>
#include <oui/file_system.h>
#include <oui/scripted_backend.h>
#include <oui/suggester.h>
#include <oui/text_edit.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>

using namespace olagarro;

namespace
{

// A document on show, with the suggester round it, and the typing a person would do.
struct Fixture
{
    Fixture()
    {
        auto window{oui::window("Test")};
        window->set_border(oui::WindowBorder::none);
        auto document{oui::text_edit()};
        editor = document.get();
        window->add(std::move(document));
        application.set_window(std::move(window));
        application.focus(*editor);
        application.run_frame();
    }

    void type(std::u32string_view text)
    {
        for(const auto character : text)
        {
            application.dispatch_event(oui::TextEvent{character});
            suggester.after_event(character);
        }
        application.run_frame();
    }

    void press(oui::Key key)
    {
        application.dispatch_event(oui::KeyEvent{key});
        suggester.after_event(std::nullopt);
        application.run_frame();
    }

    oui::Application application{oui::scripted_backend({}, {60, 15})};
    oui::TextEdit* editor{};
    // What the document's language suggests.
    std::vector<oui::Suggestion> language{{"if", "if", "if ($0) {\n}"}, {"var", "keyword", ""}};
    // Whether the document is a script of OUI's, where what its API declares is known, and whether it is a user
    // interface description.
    bool script{false};
    bool description{false};
    oui::ScriptApi api{oui::read_file(std::string{OLAGARRO_SOURCE_DIR} + "/oui/oui.d.ts").value_or("")};
    oui::Suggester suggester{
        application, [this]() -> std::optional<oui::Suggester::Context>
        { return oui::Suggester::Context{editor, &language, script ? &api : nullptr, {editor->text()}, description}; }};
};

TEST(SuggesterTests, OpensAtTwoLettersFollowsTheWordAndGoesWithIt)
{
    Fixture fixture;
    fixture.editor->set_text("value valid\n");
    fixture.editor->select_range(12, 12);
    fixture.type(U"v");
    EXPECT_FALSE(fixture.suggester.is_open()) << "one letter is not enough";
    fixture.type(U"a");
    ASSERT_TRUE(fixture.suggester.is_open());
    auto* list{dynamic_cast<oui::PopupList*>(fixture.application.popup())};
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->items().size(), 3u) << "var, valid, value";
    fixture.type(U"li");
    EXPECT_EQ(list->items().size(), 1u);
    EXPECT_EQ(list->items()[0].label, "valid");
    fixture.press(oui::Key::enter);
    EXPECT_EQ(fixture.editor->text(), "value valid\nvalid");
    EXPECT_FALSE(fixture.suggester.is_open());
    fixture.type(U" va");
    ASSERT_TRUE(fixture.suggester.is_open());
    fixture.press(oui::Key::home);
    EXPECT_FALSE(fixture.suggester.is_open()) << "the cursor left the word";
}

TEST(SuggesterTests, AskedWithNothingToSuggestItSaysSoForAMoment)
{
    Fixture fixture;
    fixture.editor->set_text("zzz");
    fixture.editor->select_range(3, 3);
    fixture.suggester.suggest();
    fixture.suggester.after_event(std::nullopt);
    ASSERT_TRUE(fixture.suggester.is_open()) << "the look after the asking leaves it";
    const auto* const list{dynamic_cast<oui::PopupList*>(fixture.application.popup())};
    ASSERT_NE(list, nullptr);
    EXPECT_TRUE(list->items().empty());
    fixture.type(U"z");
    EXPECT_FALSE(fixture.suggester.is_open()) << "the next key ends it";
}

TEST(SuggesterTests, APieceOfTextTakesTheLinesIndentationAndPutsTheCursorWhereItSays)
{
    Fixture fixture;
    fixture.editor->set_text("    ");
    fixture.editor->select_range(4, 4);
    fixture.type(U"i");
    fixture.suggester.suggest();
    ASSERT_TRUE(fixture.suggester.is_open());
    fixture.press(oui::Key::enter);
    EXPECT_EQ(fixture.editor->text(), "    if () {\n    }");
    EXPECT_EQ(fixture.editor->cursor_offset(), 8u) << "between the brackets";
}

TEST(SuggesterTests, OffItOpensOnlyWhenAsked)
{
    Fixture fixture;
    fixture.suggester.set_as_you_type(false);
    fixture.editor->set_text("value\n");
    fixture.editor->select_range(6, 6);
    fixture.type(U"val");
    EXPECT_FALSE(fixture.suggester.is_open());
    fixture.suggester.suggest();
    EXPECT_TRUE(fixture.suggester.is_open());
}

TEST(SuggesterTests, TheArrowsChooseAnyOfTheListAndItKeepsWhatTheyChose)
{
    Fixture fixture;
    fixture.editor->set_text("value valid\n");
    fixture.editor->select_range(12, 12);
    fixture.type(U"va");
    auto* list{dynamic_cast<oui::PopupList*>(fixture.application.popup())};
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->items().size(), 3u);
    fixture.press(oui::Key::down);
    fixture.press(oui::Key::down);
    EXPECT_EQ(list->selected(), 2u) << "the look after each arrow leaves the list as it was";
    const auto third{list->items()[2].label};
    fixture.press(oui::Key::enter);
    EXPECT_EQ(fixture.editor->text(), "value valid\n" + third);
}

TEST(SuggesterTests, InACallItShowsHowItIsCalledAndFollowsTheArgument)
{
    Fixture fixture;
    fixture.script = true;
    fixture.type(U"oui.writeFile(");
    auto* tip{dynamic_cast<oui::SignatureTip*>(fixture.application.popup())};
    ASSERT_NE(tip, nullptr) << "the bracket opened shows the signature";
    EXPECT_EQ(tip->signature().text, "writeFile(path: string, text: string): boolean");
    EXPECT_EQ(tip->parameter(), 0u);
    fixture.type(U"\"a\", ");
    tip = dynamic_cast<oui::SignatureTip*>(fixture.application.popup());
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(tip->parameter(), 1u) << "past a comma, the next";
    fixture.type(U"oui.readFile(");
    tip = dynamic_cast<oui::SignatureTip*>(fixture.application.popup());
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(tip->signature().text.rfind("readFile(", 0), 0u) << "the call inside";
    fixture.editor->select_range(fixture.editor->text().size(), fixture.editor->text().size());
    fixture.type(U"\"b\"))");
    EXPECT_EQ(fixture.application.popup(), nullptr) << "out of every call";
}

TEST(SuggesterTests, EscapeClosesTheSignatureUntilAnotherCall)
{
    Fixture fixture;
    fixture.script = true;
    fixture.type(U"oui.readFile(");
    ASSERT_NE(dynamic_cast<oui::SignatureTip*>(fixture.application.popup()), nullptr);
    fixture.press(oui::Key::escape);
    EXPECT_EQ(fixture.application.popup(), nullptr);
    fixture.type(U"\"x\"");
    EXPECT_EQ(fixture.application.popup(), nullptr) << "not back while in the same call";
    fixture.editor->select_range(fixture.editor->text().size(), fixture.editor->text().size());
    fixture.type(U")\noui.exists(");
    EXPECT_NE(dynamic_cast<oui::SignatureTip*>(fixture.application.popup()), nullptr) << "back in another";
}

TEST(SuggesterTests, OnlyInScriptsAndTheListComesFirst)
{
    Fixture fixture;
    fixture.type(U"oui.readFile(");
    EXPECT_EQ(fixture.application.popup(), nullptr) << "not a script";
    fixture.script = true;
    fixture.editor->set_text("const path = 1;\n");
    fixture.editor->select_range(16, 16);
    fixture.type(U"oui.readFile(pa");
    EXPECT_TRUE(fixture.suggester.is_open()) << "the list of what to write over the signature";
    fixture.press(oui::Key::escape);
    EXPECT_NE(dynamic_cast<oui::SignatureTip*>(fixture.application.popup()), nullptr)
        << "with the list gone, the signature is back";
}

} // namespace

TEST(SuggesterTests, ADocumentClosedUnderTheListIsNotWrittenInNorTheOneMadeWhereItWas)
{
    oui::Application application{oui::scripted_backend({}, {60, 15})};
    application.set_content(oui::label("documents"));
    std::optional<oui::TextEdit> slot;
    slot.emplace();
    slot->set_text("value valid\n");
    slot->arrange({0, 0, 60, 15});
    slot->select_range(12, 12);
    const std::vector<oui::Suggestion> language;
    oui::Suggester suggester{application, [&slot, &language]() -> std::optional<oui::Suggester::Context>
                             {
                                 if(!slot)
                                 {
                                     return std::nullopt;
                                 }
                                 return oui::Suggester::Context{&*slot, &language, nullptr, {slot->text()}};
                             }};
    for(const auto character : std::u32string{U"va"})
    {
        slot->handle_event(oui::TextEvent{character});
        suggester.after_event(character);
    }
    ASSERT_TRUE(suggester.is_open());

    // The tab closes with the list open, and the next document is made where it was.
    slot.reset();
    slot.emplace();
    slot->set_text("zzz");
    application.dispatch_event(oui::KeyEvent{oui::Key::enter});
    EXPECT_EQ(slot->text(), "zzz") << "the suggestion was for the closed document";
}

TEST(SuggesterTests, ALetterThatWentElsewhereOpensNothing)
{
    // Alt+T and U open an Interactive UI tab from the menu: the U picks the menu's entry, and the description, focused
    // once the tab opens, was not written in.
    Fixture fixture;
    fixture.description = true;
    fixture.editor->set_text("Window \"Hi\"\n  Label \"hola\"\n");
    for(const auto cursor : {std::size_t{0}, std::size_t{3}, fixture.editor->text().size()})
    {
        fixture.editor->select_range(cursor, cursor);
        fixture.suggester.after_event(U'u');
        fixture.application.run_frame();
        EXPECT_FALSE(fixture.suggester.is_open()) << cursor;
    }
}

TEST(SuggesterTests, InADescriptionASpaceAfterAWidgetOpensWhatItTakes)
{
    Fixture fixture;
    fixture.description = true;
    fixture.editor->set_text("Window \"Hi\"\n  Label \"hola mundo\"");
    fixture.editor->select_range(fixture.editor->text().size(), fixture.editor->text().size());
    fixture.type(U" ");
    ASSERT_TRUE(fixture.suggester.is_open()) << "the space opens it, with nothing written yet";
    auto* list{dynamic_cast<oui::PopupList*>(fixture.application.popup())};
    ASSERT_NE(list, nullptr);
    const auto has{[list](std::string_view label)
                   {
                       return std::any_of(list->items().begin(), list->items().end(),
                                          [label](const auto& item) { return item.label == label; });
                   }};
    EXPECT_TRUE(has("buddy"));
    EXPECT_TRUE(has("wrap"));
    EXPECT_FALSE(has("gap"));
    // It follows what is written: then the value, after the =.
    fixture.type(U"backg");
    ASSERT_TRUE(fixture.suggester.is_open());
    fixture.press(oui::Key::enter);
    EXPECT_EQ(fixture.editor->text(), "Window \"Hi\"\n  Label \"hola mundo\" background=");
    fixture.type(U"table_b");
    ASSERT_TRUE(fixture.suggester.is_open());
    fixture.press(oui::Key::enter);
    EXPECT_EQ(fixture.editor->text(), "Window \"Hi\"\n  Label \"hola mundo\" background=table_background");
}
