#include "session_entry.h"

#include <gtest/gtest.h>

TEST(OuiCodeSessionEntryTests, AFileIsItsPathAsItIs)
{
    EXPECT_EQ(session_entry({"", {"/home/ana/notes.txt"}}), "/home/ana/notes.txt");
    const auto tab{session_tab("/home/ana/notes.txt")};
    EXPECT_EQ(tab.kind, "");
    EXPECT_EQ(tab.parts, (std::vector<std::string>{"/home/ana/notes.txt"}));
    // A drive's colon is not a kind.
    EXPECT_EQ(session_tab("C:/notes.txt").kind, "");
}

TEST(OuiCodeSessionEntryTests, AnyTextComesBackAsItWas)
{
    // What a list of the settings or a line of them would break, and what marks the parts.
    const SessionTab tab{"interactive", {"/a;b/script.js", "", "let x = 1;\n\tx %= 2;\r\nreturn x;"}};
    const auto entry{session_entry(tab)};
    EXPECT_EQ(entry.find(';'), std::string::npos);
    EXPECT_EQ(entry.find('\n'), std::string::npos);
    const auto back{session_tab(entry)};
    EXPECT_EQ(back.kind, "interactive");
    EXPECT_EQ(back.parts, tab.parts);
}

TEST(OuiCodeSessionEntryTests, AKindWithNothingMoreHasOneEmptyPart)
{
    EXPECT_EQ(session_entry({"terminal", {}}), "terminal:");
    EXPECT_EQ(session_tab("terminal:").kind, "terminal");
}
