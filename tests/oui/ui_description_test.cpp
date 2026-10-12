#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/declarative.h>
#include <oui/label.h>
#include <oui/line_edit.h>
#include <oui/progress_bar.h>
#include <oui/tab_view.h>
#include <oui/text_edit.h>
#include <oui/translation.h>
#include <oui/ui.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <stdexcept>
#include <string>

using namespace olagarro;

TEST(OuiDeclarativeTests, ReadsPropertiesFlagsAndQuotedValues)
{
    const auto result{oui::parse_oui("Window \"Find in Files\" context=FindInFiles width=60\n"
                                     "  # What to look for.\n"
                                     "  LineEdit id=query placeholder=\"Every file, or \\\"*.cpp\\\"\"\n"
                                     "  Button \"OK\" id=ok default\n")};
    ASSERT_TRUE(result.succeeded()) << result.diagnostics.front().message;
    const auto& window{*result.root};
    EXPECT_EQ(window.text, std::optional<std::string>{"Find in Files"});
    EXPECT_EQ(window.property("context"), std::optional<std::string_view>{"FindInFiles"});
    EXPECT_EQ(window.property("width"), std::optional<std::string_view>{"60"});
    ASSERT_EQ(window.children.size(), 2u);
    const auto& query{window.children[0]};
    EXPECT_EQ(query.type, "LineEdit");
    EXPECT_FALSE(query.text);
    EXPECT_EQ(query.property("placeholder"), std::optional<std::string_view>{"Every file, or \"*.cpp\""});
    EXPECT_EQ(query.line, 3u);
    EXPECT_EQ(query.column, 3u);
    const auto& ok{window.children[1]};
    EXPECT_TRUE(ok.has("default"));
    EXPECT_FALSE(ok.has("cancel"));
    EXPECT_EQ(ok.property("default"), std::optional<std::string_view>{""});
}

TEST(OuiDeclarativeTests, SaysWhatIsWrongAndWhere)
{
    const auto where{[](const char* source)
                     {
                         const auto result{oui::parse_oui(source)};
                         return result.diagnostics.empty() ? std::string{"nothing"}
                                                           : std::to_string(result.diagnostics.front().line) + ":" +
                                                                 std::to_string(result.diagnostics.front().column) +
                                                                 " " + result.diagnostics.front().message;
                     }};
    EXPECT_EQ(where("Button id=ok \"OK\"\n"), "1:14 The text goes right after the type.");
    EXPECT_EQ(where("Label \"open\n"), "1:7 A quote is not closed.");
    EXPECT_EQ(where("Label Bad=1\n"), "1:7 Expected a property, as name=value or a name alone.");
    EXPECT_EQ(where("Label id=a id=b\n"), "1:12 The property id is given twice.");
    EXPECT_EQ(where("Window\nWindow\n"), "2:1 A document can only have one root widget.");
}

namespace
{

constexpr std::string_view find_dialog{R"(Window "Find in Files" context=FindInFiles width=60
  Label "&Find:" id=find_label buddy=query
  LineEdit id=query placeholder="Text to find"
  CheckButton "&Whole word" id=whole_word checked
  Label "Options" heading
  Grid columns=content,fill gap=tight
    Label "&In files:" buddy=files
    LineEdit id=files
  VSpacer height=1
  Row centered gap=tight
    Button "Find" id=find default
    Button "Cancel" id=cancel
)"};

// What building the description says is wrong, with where, or "built".
[[nodiscard]] std::string problem_of(std::string_view description)
{
    try
    {
        static_cast<void>(oui::build_ui(description));
        return "built";
    }
    catch(const oui::UiError& error)
    {
        return error.what();
    }
}

} // namespace

TEST(OuiUiTests, BuildsWhatTheDescriptionSaysAndFindsItsWidgetsByTheirIds)
{
    auto ui{oui::build_ui(find_dialog)};
    auto& query{ui.get<oui::LineEdit>("query")};
    EXPECT_TRUE(ui.get<oui::CheckButton>("whole_word").is_checked());
    EXPECT_TRUE(ui.get<oui::Button>("find").is_default());
    EXPECT_FALSE(ui.get<oui::Button>("cancel").is_default());
    EXPECT_EQ(ui.get<oui::Button>("find").text(), "Find");
    // A label is the buddy of the field it names, which Alt and its letter go to.
    EXPECT_EQ(ui.get<oui::Label>("find_label").buddy(), &query);
    auto window{ui.take_window()};
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->title(), "Find in Files");
}

TEST(OuiUiTests, ALabelWithNoBuddyShowsItsTextWithoutTheLettersMark)
{
    // Its & has no field to take Alt and the letter to: it goes, and && is an & written.
    auto ui{oui::build_ui("Column\n  Label \"&Pattern:\" id=alone\n  Label \"R&&D\" id=written\n"
                          "  Label \"&Name:\" id=named buddy=name\n  LineEdit id=name\n",
                          oui::UiTexts::as_written)};
    EXPECT_EQ(ui.get<oui::Label>("alone").text(), "Pattern:");
    EXPECT_EQ(ui.get<oui::Label>("written").text(), "R&D");
    EXPECT_EQ(ui.get<oui::Label>("named").text(), "&Name:") << "with a buddy, the letter goes to it";
}

TEST(OuiUiTests, TranslatesItsTextsUnderTheContextItsRootNames)
{
    oui::TranslationCatalog catalog{"es", "Español", {}};
    catalog.contexts["FindInFiles"]["Find in Files"] = "Buscar en archivos";
    catalog.contexts["FindInFiles"]["Text to find"] = "Texto que buscar";
    oui::remove_translations();
    oui::install_translation(oui::Translation{catalog});
    auto ui{oui::build_ui(find_dialog)};
    oui::remove_translations();
    EXPECT_EQ(ui.take_window()->title(), "Buscar en archivos");

    // What there is to translate, for the catalogs to cover.
    const auto texts{oui::ui_texts(*oui::parse_oui(find_dialog).root)};
    ASSERT_GE(texts.size(), 3u);
    EXPECT_EQ(texts[0].context, "FindInFiles");
    EXPECT_EQ(texts[0].text, "Find in Files");
    EXPECT_EQ(texts[2].text, "Text to find");
    EXPECT_EQ(texts[2].line, 3u);
}

TEST(OuiUiTests, SaysWhatIsWrongWithADescriptionAndWhere)
{
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  Slider\n"), "2:3: There is no widget called Slider.");
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  Label \"B\" colour=red\n"),
              "2:13: A Label has no property colour.");
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  Label \"B\" buddy=nobody\n"), "2:3: No widget has the id nobody.");
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  LineEdit id=a\n  LineEdit id=a\n"),
              "3:3: The id a is given twice.");
    EXPECT_EQ(problem_of("Window \"A\"\n"), "1:1: Texts to translate need context=, the place they belong to.");
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  Grid columns=content,wide\n"),
              "2:3: A column is content, fill, flex or a number of cells, not wide.");
    EXPECT_EQ(problem_of("Label \"A\n"), "1:7: A quote is not closed.");
}

// The column of a property after a quoted value counted the value without its quotes and escapes.
TEST(OuiUiTests, SaysTheColumnOfAPropertyAfterQuotedValues)
{
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  LineEdit placeholder=\"a \\\"b\\\" c\" colour=red\n"),
              "2:36: A LineEdit has no property colour.");
    EXPECT_EQ(problem_of("Window \"A\" context=X\n  Label \"say \\\"hi\\\"\" colour=red\n"),
              "2:22: A Label has no property colour.");
}

// A Tab took hidden and tooltip, and did nothing with them: the tooltip is its title's now, and hidden is wrong.
TEST(OuiUiTests, ATabsTooltipIsItsTitlesAndATabCannotBeHidden)
{
    EXPECT_EQ(problem_of("Tabs\n  Tab \"One\" untranslated hidden\n"), "2:3: A Tab cannot be hidden.");
    auto ui{
        oui::build_ui("Tabs id=tabs\n  Tab \"One\" untranslated tooltip=\"The first\"\n  Tab \"Two\" untranslated\n",
                      oui::UiTexts::as_written)};
    auto& tabs{ui.get<oui::TabView>("tabs")};
    tabs.arrange({0, 0, 40, 10});
    EXPECT_EQ(tabs.tooltip_at({tabs.bounds().x + 2, tabs.bounds().y}), std::optional<std::string>{"The first"});
    EXPECT_EQ(tabs.tooltip_at({tabs.bounds().x + 9, tabs.bounds().y}), std::nullopt);
}

TEST(OuiUiTests, AskingForAWidgetThatIsNotThereFailsAtOnce)
{
    auto ui{oui::build_ui(find_dialog)};
    EXPECT_THROW(static_cast<void>(ui.get<oui::LineEdit>("nothing")), std::logic_error);
    EXPECT_THROW(static_cast<void>(ui.get<oui::Button>("query")), std::logic_error);
}

TEST(OuiUiTests, EveryDescriptionInTheRepositoryBuilds)
{
    // As each program builds its own when it shows it: one that is broken fails here, not in front of someone.
    const std::filesystem::path source{OLAGARRO_SOURCE_DIR};
    for(const auto* const folder : {"oui", "examples"})
    {
        for(const auto& entry : std::filesystem::recursive_directory_iterator{source / folder})
        {
            if(entry.path().extension() != ".oui")
            {
                continue;
            }
            std::ifstream file{entry.path(), std::ios::binary};
            std::stringstream text;
            text << file.rdbuf();
            try
            {
                // A program's names the context its texts are translated under; a script's shows them as written.
                const auto parsed{oui::parse_oui(text.str())};
                const auto translated{parsed.root && parsed.root->has("context")};
                static_cast<void>(
                    oui::build_ui(text.str(), translated ? oui::UiTexts::translated : oui::UiTexts::as_written));
            }
            catch(const oui::UiError& error)
            {
                ADD_FAILURE() << entry.path().filename().string() << ":" << error.what();
            }
        }
    }
}

TEST(OuiUiTests, BuildsPanelsWithTabsSplitsAndPlacesForTheCodesOwnWidgets)
{
    auto ui{oui::build_ui(R"(Grid context=Tasks columns=fill rows=content,flex
  Split vertical ratio=0.25
    Tabs id=tabs fixed
      Tab "Pending"
      Tab "Done"
        TextEdit id=done read_only wrap
    Slot id=list
  Row
    Button "Stop" id=stop compact flat hidden
    Button "↑" untranslated compact
    HSpacer width=2
    MarkdownView id=page background=reading_background foreground=reading_foreground
)")};
    auto& tabs{ui.get<oui::TabView>("tabs")};
    EXPECT_EQ(tabs.child_count(), 2u);
    EXPECT_TRUE(ui.get<oui::TextEdit>("done").is_read_only());
    EXPECT_FALSE(ui.get<oui::Button>("stop").is_visible());
    EXPECT_TRUE(ui.is_slot("list"));
    EXPECT_FALSE(ui.is_slot("done"));
    // The place the code fills: what it puts there takes the place's id.
    auto& list{ui.put("list", oui::label("mine"))};
    EXPECT_EQ(&ui.get<oui::Label>("list"), &list);
    EXPECT_FALSE(ui.is_slot("list"));
    EXPECT_EQ(list.parent(), static_cast<const oui::Container*>(tabs.parent()));
    // A widget can leave the description for a place of the code's.
    auto stop{ui.take("stop")};
    EXPECT_EQ(stop->parent(), nullptr);
    // The texts to translate leave out those written to show as they are.
    const auto texts{oui::ui_texts(*oui::parse_oui("Row context=X\n  Button \"↑\" untranslated\n"
                                                   "  Button \"Go\"\n")
                                        .root)};
    ASSERT_EQ(texts.size(), 1u);
    EXPECT_EQ(texts[0].text, "Go");
}

TEST(OuiUiTests, AWindowHoldsItsOnlyWidgetAsItIsAndTakesItsShapeFromTheDescription)
{
    auto ui{oui::build_ui("Window \"OUI Code\" untranslated border=none keep_on_escape\n  TextEdit id=text\n")};
    auto& text{ui.get<oui::TextEdit>("text")};
    auto window{ui.take_window()};
    EXPECT_EQ(&window->child_at(0), &text);
    EXPECT_EQ(window->border(), oui::WindowBorder::none);
    EXPECT_FALSE(window->closes_on_escape());
}

TEST(OuiUiTests, SaysWhatIsWrongWithPanelsAndWhere)
{
    EXPECT_EQ(problem_of("Split\n  Label\n  Label\n  Label\n"), "4:3: A Split holds two widgets.");
    EXPECT_EQ(problem_of("Tabs\n  Label\n"), "2:3: A Tabs holds Tab, not Label.");
    EXPECT_EQ(problem_of("Row context=X\n  Tab \"A\"\n"), "2:3: A Tab goes in a Tabs.");
    EXPECT_EQ(problem_of("Row\n  Label background=paper\n"), "2:9: There is no colour paper in the theme.");
    EXPECT_EQ(problem_of("Row\n  Row context=X\n"), "2:7: Only the root names a context.");
    EXPECT_EQ(problem_of("Slot id=x\n"), "1:1: A Slot goes inside another widget.");
    EXPECT_EQ(problem_of("Row\n  Slot\n"), "2:3: A Slot needs an id, for the code to fill it.");
    EXPECT_EQ(problem_of("Grid rows=content,tall\n"),
              "1:1: A row is content, fill, flex or a number of cells, not tall.");
}

TEST(OuiUiTests, EveryColourOfTheThemeCanBeNamed)
{
    // Read from theme.h, so a colour added there and not to the names fails here.
    std::ifstream file{std::filesystem::path{OLAGARRO_SOURCE_DIR} / "oui" / "include" / "oui" / "theme.h"};
    std::string line;
    std::size_t colours{};
    const std::string_view field{"    Color "};
    while(std::getline(file, line) && line != "struct Theme")
    {
    }
    while(std::getline(file, line) && line != "};")
    {
        if(line.rfind(field, 0) != 0)
        {
            continue;
        }
        const auto name{line.substr(field.size(), line.find('{') - field.size())};
        ++ colours;
        EXPECT_EQ(problem_of("Row\n  Label background=" + name + "\n"), "built") << name;
    }
    EXPECT_GT(colours, 80u);
}

TEST(OuiUiTests, AProgressBarStartsWithTheValueAndTextItIsGiven)
{
    auto ui{oui::build_ui("Column\n  ProgressBar id=progress value=0.25 text=\"1 of 4\"\n", oui::UiTexts::as_written)};
    auto& bar{ui.get<oui::ProgressBar>("progress")};
    EXPECT_EQ(bar.value(), 0.25);
    EXPECT_EQ(bar.text(), "1 of 4");
}

TEST(OuiUiTests, ALineEditMarkedPasswordHidesWhatIsTyped)
{
    auto ui{oui::build_ui("Row\n  LineEdit id=secret password\n  LineEdit id=name\n", oui::UiTexts::as_written)};
    EXPECT_TRUE(ui.get<oui::LineEdit>("secret").is_password());
    EXPECT_FALSE(ui.get<oui::LineEdit>("name").is_password());
}

TEST(OuiUiTests, TextsCanShowAsTheyAreWrittenWithoutAContext)
{
    oui::TranslationCatalog catalog{"es", "Español", {}};
    catalog.contexts[""]["Find"] = "Buscar";
    oui::remove_translations();
    oui::install_translation(oui::Translation{catalog});
    auto ui{oui::build_ui("Row\n  Button \"Find\" id=find\n  LineEdit id=query placeholder=\"Find\"\n",
                          oui::UiTexts::as_written)};
    oui::remove_translations();
    EXPECT_EQ(ui.get<oui::Button>("find").text(), "Find");
}

TEST(OuiUiTests, AnErrorSaysItsLineAndColumnApartToo)
{
    // For a tool to go there: the line and column of what is wrong, from 1, besides the message.
    for(const auto* const description : {"Window \"A\" context=X\n  Label \"B\" colour=red\n", "Row\n  Label \"B\n"})
    {
        try
        {
            static_cast<void>(oui::build_ui(description, oui::UiTexts::as_written));
            ADD_FAILURE() << description;
        }
        catch(const oui::UiError& error)
        {
            EXPECT_EQ(error.line(), 2u);
            EXPECT_EQ(std::to_string(error.line()) + ":" + std::to_string(error.column()) + ": " + error.message(),
                      error.what());
        }
    }
}

namespace
{

// The labels suggested with the cursor at the end of `description`, and where what they replace starts.
[[nodiscard]] std::pair<std::size_t, std::vector<std::string>> suggested(std::string_view description)
{
    const auto found{oui::ui_suggestions(description, description.size())};
    std::vector<std::string> labels;
    for(const auto& each : found.items)
    {
        labels.push_back(each.label);
    }
    return {found.start, labels};
}

[[nodiscard]] bool has(const std::vector<std::string>& labels, std::string_view label)
{
    return std::find(labels.begin(), labels.end(), label) != labels.end();
}

} // namespace

TEST(OuiUiTests, SuggestsWhatFitsWhereTheCursorIs)
{
    // After a widget and its text, the properties it takes and has not been given, and those every widget takes.
    const std::string label{"Window \"A\"\n  Label \"hola mundo\" bold "};
    const auto [start, properties]{suggested(label)};
    EXPECT_EQ(start, label.size());
    for(const auto* const each : {"buddy", "heading", "wrap", "background", "id", "hidden", "untranslated"})
    {
        EXPECT_TRUE(has(properties, each)) << each;
    }
    EXPECT_FALSE(has(properties, "bold")) << "given already";
    EXPECT_FALSE(has(properties, "gap")) << "a Label has none";
    EXPECT_FALSE(has(properties, "context")) << "only the root names one";

    // What a valued property goes in as, and a flag alone.
    const auto items{oui::ui_suggestions("Row ", 4).items};
    const auto gap{std::find_if(items.begin(), items.end(), [](const auto& each) { return each.label == "gap"; })};
    ASSERT_NE(gap, items.end());
    EXPECT_EQ(gap->text, "gap=$0");

    // At the start of a line, the widgets, those that show a text with its quotes.
    const auto widgets{oui::ui_suggestions("Column\n  Bu", 11)};
    EXPECT_EQ(widgets.start, 9u);
    ASSERT_EQ(widgets.items.size(), 1u);
    EXPECT_EQ(widgets.items.front().text, "Button \"$0\"");

    // After name=, its values: from a list, the theme's colours, the ids in the description.
    EXPECT_EQ(suggested("Row gap=t"), (std::pair{std::size_t{8}, std::vector<std::string>{"tight"}}));
    EXPECT_EQ(suggested("Grid columns=content,f"),
              (std::pair{std::size_t{21}, std::vector<std::string>{"fill", "flex"}}));
    EXPECT_TRUE(has(suggested("Label background=table_b").second, "table_background"));
    EXPECT_EQ(suggested("Column\n  LineEdit id=name\n  Label \"N\" buddy=").second, std::vector<std::string>{"name"});

    // Nothing in a text, a comment, or after a widget there is not.
    EXPECT_TRUE(suggested("Label \"hola ").second.empty());
    EXPECT_TRUE(suggested("# A comment ").second.empty());
    EXPECT_TRUE(suggested("Slider ").second.empty());
}

TEST(OuiUiTests, AWidgetTellsWhatItDoesToAPointerRestingOnIt)
{
    oui::TranslationCatalog catalog{"es", "Español", {}};
    catalog.contexts["Tools"]["Runs it again"] = "Lo ejecuta de nuevo";
    oui::remove_translations();
    oui::install_translation(oui::Translation{catalog});
    auto ui{oui::build_ui("Row context=Tools\n  Button \"Again\" id=again tooltip=\"Runs it again\"\n")};
    oui::remove_translations();
    EXPECT_EQ(ui.get<oui::Button>("again").tooltip_at({0, 0}), std::optional<std::string>{"Lo ejecuta de nuevo"});
    // A text to translate, as the others are; and one an editor suggests.
    const auto texts{oui::ui_texts(*oui::parse_oui("Row context=X\n  Label tooltip=\"Hint\"\n").root)};
    ASSERT_EQ(texts.size(), 1u);
    EXPECT_EQ(texts[0].text, "Hint");
    EXPECT_TRUE(has(suggested("Row\n  Label ").second, "tooltip"));
}
