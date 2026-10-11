#include <oui/cell_buffer.h>
#include <oui/markdown_view.h>
#include <oui/text_metrics.h>
#include <oui/text_layout.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace olagarro;

namespace
{

using oui::MarkdownView;
using Role = MarkdownView::Role;

// The text of each laid out line.
std::vector<std::string> texts(const std::vector<MarkdownView::Line>& lines)
{
    std::vector<std::string> result;
    for(const auto& line : lines)
    {
        std::string text;
        for(const auto& run : line)
        {
            text += run.text;
        }
        result.push_back(text);
    }
    return result;
}

// The role of the run holding `text` on any line.
std::optional<Role> role_of(const std::vector<MarkdownView::Line>& lines, const std::string& text)
{
    for(const auto& line : lines)
    {
        for(const auto& run : line)
        {
            if(run.text == text)
            {
                return run.role;
            }
        }
    }
    return std::nullopt;
}

std::unique_ptr<MarkdownView> view_of(std::string markdown)
{
    return oui::markdown_view(std::move(markdown));
}

TEST(OuiMarkdownViewTests, LaysOutHeadingsAndParagraphsOneBlankLineApart)
{
    const auto lines{view_of("# Title\n\nFirst paragraph\nstill the first.\n\n## Section\nText")->layout(40)};
    EXPECT_EQ(texts(lines), (std::vector<std::string>{"Title", "═════", "", "First paragraph still the first.", "",
                                                      "Section", "", "Text"}));
    EXPECT_EQ(lines[0][0].role, Role::heading);
}

TEST(OuiMarkdownViewTests, ShowsTabsInProseAsASpace)
{
    // A list of names and letters parted by tabs, as people write them in plain text; code keeps its tabs as
    // columns, as KeepsCodeBlocksAsTheyAre shows.
    const auto lines{view_of("Medvedka\tM\nBulava\t\tM")->layout(60)};
    EXPECT_EQ(texts(lines), (std::vector<std::string>{"Medvedka M Bulava M"}));
}

TEST(OuiMarkdownViewTests, WrapsParagraphsAtWordsAndBreaksWordsWiderThanTheLine)
{
    const auto lines{view_of("one two three four five\n\nabcdefghijkl")->layout(10)};
    EXPECT_EQ(texts(lines), (std::vector<std::string>{"one two", "three four", "five", "", "abcdefghij", "kl"}));
}

TEST(OuiMarkdownViewTests, FormatsInlineTextButLeavesSnakeCaseAlone)
{
    const auto lines{view_of("Use **bold**, *it*, `code`, [the docs](http://x) and toggle_line_comment.")->layout(80)};
    EXPECT_EQ(role_of(lines, "bold"), Role::strong);
    EXPECT_EQ(role_of(lines, "it"), Role::emphasis);
    EXPECT_EQ(role_of(lines, "code"), Role::code);
    EXPECT_EQ(role_of(lines, "the"), Role::link);
    EXPECT_EQ(texts(lines).front(), "Use bold, it, code, the docs and toggle_line_comment.");
}

TEST(OuiMarkdownViewTests, IndentsListsUnderTheirMarker)
{
    const auto lines{view_of("- first item that wraps\n- second\n  - nested\n1. numbered")->layout(16)};
    EXPECT_EQ(texts(lines),
              (std::vector<std::string>{"• first item", "  that wraps", "• second", "  • nested", "1. numbered"}));
}

TEST(OuiMarkdownViewTests, KeepsCodeBlocksAsTheyAre)
{
    const auto lines{view_of("Before\n\n```\nint  x{1};\n\tindented\n```\nAfter")->layout(40)};
    EXPECT_EQ(texts(lines), (std::vector<std::string>{"Before", "", "int  x{1};", "    indented", "", "After"}));
    EXPECT_EQ(lines[2][0].role, Role::code_block);
}

TEST(OuiMarkdownViewTests, DrawsTablesInColumnsAndNarrowsThemToFit)
{
    const auto markdown{"| Key | Action |\n|-----|--------|\n| Ctrl+S | Save the file |\n| F5 | Copy |"};
    const auto lines{view_of(markdown)->layout(40)};
    // The rule runs under the header, its cross under the column separator.
    EXPECT_EQ(texts(lines), (std::vector<std::string>{"Key    │ Action       ", "───────┼──────────────",
                                                      "Ctrl+S │ Save the file", "F5     │ Copy         "}));
    EXPECT_EQ(lines[0][0].role, Role::table_header);
    // In 16 columns the widest column gives way and its text is cut.
    for(const auto& line : view_of(markdown)->layout(16))
    {
        EXPECT_LE(oui::text_width(texts({line}).front()), 16);
    }
}

TEST(OuiMarkdownViewTests, ScrollsWithTheKeysWithinItsText)
{
    std::string markdown;
    for(int line{}; line < 30; ++ line)
    {
        markdown += "Line " + std::to_string(line) + "\n\n";
    }
    auto view{view_of(markdown)};
    view->arrange({0, 0, 20, 10});
    EXPECT_FALSE(view->handle_event(oui::KeyEvent{oui::Key::up}));
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::page_down}));
    EXPECT_EQ(view->scroll_offset(), 9);
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::end}));
    EXPECT_EQ(view->scroll_offset(), 59 - 10);
    EXPECT_FALSE(view->handle_event(oui::KeyEvent{oui::Key::down}));
}

TEST(OuiMarkdownViewTests, ALinkKnowsWhereItLeadsAndAClickOnItSays)
{
    auto view{view_of("See [the guide](guide.md#start) here.")};
    view->arrange({0, 0, 40, 5});
    const auto lines{view->layout(38)};
    ASSERT_FALSE(lines.empty());
    std::string target;
    for(const auto& run : lines.front())
    {
        if(run.role == Role::link)
        {
            target = run.target;
        }
    }
    EXPECT_EQ(target, "guide.md#start");
    std::vector<std::string> followed;
    view->on_link([&followed](std::string_view where) { followed.emplace_back(where); });
    // The text starts a column in: "See " takes columns 1 to 4, the link 5 to 13.
    EXPECT_TRUE(view->handle_event(oui::MouseEvent{6, 0, true}));
    EXPECT_EQ(followed, (std::vector<std::string>{"guide.md#start"}));
    static_cast<void>(view->handle_event(oui::MouseEvent{2, 0, true}));
    EXPECT_EQ(followed.size(), 1u) << "plain text leads nowhere";
}

TEST(OuiMarkdownViewTests, TabGoesFromLinkToLinkAndEnterFollowsTheOneThereIs)
{
    auto view{view_of("[one](a.md) and [two](b.md)")};
    view->arrange({0, 0, 40, 5});
    std::vector<std::string> followed;
    view->on_link([&followed](std::string_view where) { followed.emplace_back(where); });
    EXPECT_FALSE(view->handle_event(oui::KeyEvent{oui::Key::enter})) << "no link chosen yet";
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(followed, (std::vector<std::string>{"a.md", "b.md"}));
    // Shift+Tab goes back; past the last, Tab is someone else's, as a dialog's next field.
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::shift_tab, false, false, true}));
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::enter}));
    EXPECT_EQ(followed.back(), "a.md");
    EXPECT_TRUE(view->handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_FALSE(view->handle_event(oui::KeyEvent{oui::Key::tab}));
}

TEST(OuiMarkdownViewTests, ALinkToASectionGoesToItsHeadingItself)
{
    std::string markdown{"[Down there](#cómo-se-pinta)\n\n"};
    for(int line{}; line < 30; ++ line)
    {
        markdown += "Line " + std::to_string(line) + "\n\n";
    }
    // Enough after it for the view to show it at the top.
    markdown += "## Cómo se *pinta*\n\n";
    for(int line{}; line < 10; ++ line)
    {
        markdown += "After " + std::to_string(line) + "\n\n";
    }
    auto view{view_of(markdown)};
    view->arrange({0, 0, 40, 10});
    std::vector<std::string> followed;
    view->on_link([&followed](std::string_view where) { followed.emplace_back(where); });
    EXPECT_TRUE(view->handle_event(oui::MouseEvent{3, 0, true}));
    EXPECT_TRUE(followed.empty()) << "the view goes there on its own";
    const auto lines{texts(view->layout(38))};
    ASSERT_GT(lines.size(), static_cast<std::size_t>(view->scroll_offset()));
    EXPECT_EQ(lines[static_cast<std::size_t>(view->scroll_offset())], "Cómo se pinta");
    view->scroll_to(0);
    EXPECT_TRUE(view->scroll_to_anchor("cómo-se-pinta"));
    EXPECT_GT(view->scroll_offset(), 0);
    EXPECT_FALSE(view->scroll_to_anchor("nowhere"));
}

// Headings with the same name are told apart as GitHub tells them: the second "-1", the third "-2".
TEST(OuiMarkdownViewTests, NumbersTheSectionsOfHeadingsWithTheSameName)
{
    std::string markdown;
    for(const auto* const heading : {"Split", "Other", "Split", "Split"})
    {
        markdown += std::string{"## "} + heading + "\n\n";
        for(int line{}; line < 20; ++ line)
        {
            markdown += "Text\n\n";
        }
    }
    auto view{view_of(markdown)};
    view->arrange({0, 0, 40, 10});
    ASSERT_TRUE(view->scroll_to_anchor("split"));
    const auto first{view->scroll_offset()};
    ASSERT_TRUE(view->scroll_to_anchor("split-1"));
    const auto second{view->scroll_offset()};
    ASSERT_TRUE(view->scroll_to_anchor("split-2"));
    EXPECT_GT(second, first);
    EXPECT_GT(view->scroll_offset(), second);
    EXPECT_FALSE(view->scroll_to_anchor("split-3"));
}

} // namespace

namespace
{

// A proportional font as a window's: text half as wide as the cells it would take in a terminal.
class NarrowFont final : public oui::TextMetrics
{
  public:
    [[nodiscard]] int cell_width() const override { return 8; }
    [[nodiscard]] int width(std::string_view text) const override { return 4 * oui::text_width(text); }
};

} // namespace

TEST(OuiMarkdownViewTests, ProseFillsTheLineAsTheFontItIsDrawnInMeasuresIt)
{
    std::string paragraph;
    for(int word{}; word < 60; ++ word)
    {
        paragraph += word % 3 == 0 ? "longer " : "word ";
    }
    const auto view{oui::markdown_view(paragraph)};
    const auto in_cells{view->measure({42, 1000}).height};
    const NarrowFont narrow;
    oui::set_text_metrics(&narrow);
    const auto in_a_narrow_font{view->measure({42, 1000}).height};
    const auto lines{view->layout(40)};
    oui::set_text_metrics(nullptr);
    // And never a word cut in two to do it.
    for(const auto& line : texts(lines))
    {
        std::string words{line};
        for(std::size_t start{}; start < words.size();)
        {
            const auto end{std::min(words.find(' ', start), words.size())};
            const auto word{words.substr(start, end - start)};
            EXPECT_TRUE(word == "word" || word == "longer") << "\"" << word << "\" in \"" << line << "\"";
            start = end + 1;
        }
    }
    // Half as wide, about half the lines: a line takes as many words as fit, not as many as cells would.
    EXPECT_LE(in_a_narrow_font, in_cells / 2 + 1);
}
