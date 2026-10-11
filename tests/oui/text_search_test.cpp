#include <oui/text_search.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

using Matches = std::vector<std::pair<std::size_t, std::size_t>>;

TEST(OuiTextSearchTests, FindsEveryMatchAsByteOffsets)
{
    const oui::TextSearch search{"ab"};
    EXPECT_EQ(search.find_all("ab xab ñab"), (Matches{{0, 2}, {4, 6}, {9, 11}}));
    EXPECT_TRUE(search.find_all("AB").empty());
}

TEST(OuiTextSearchTests, FoldsCaseAndKeepsToWholeWordsWhenAsked)
{
    oui::SearchOptions options;
    options.match_case = false;
    options.whole_word = true;
    const oui::TextSearch search{"Cat", options};
    EXPECT_EQ(search.find_all("cat category CAT tomcat"), (Matches{{0, 3}, {13, 16}}));
}

TEST(OuiTextSearchTests, CompilesARegularExpressionOnceForEveryText)
{
    oui::SearchOptions options;
    options.regex = true;
    const oui::TextSearch search{"[0-9]+", options};
    ASSERT_TRUE(search.is_valid());
    EXPECT_EQ(search.find_all("a1 b22"), (Matches{{1, 2}, {4, 6}}));
    EXPECT_EQ(search.find_all("333"), (Matches{{0, 3}}));
}

TEST(OuiTextSearchTests, AnEmptyNeedleOrABrokenExpressionFindsNothing)
{
    oui::SearchOptions options;
    options.regex = true;
    const oui::TextSearch broken{"(", options};
    EXPECT_FALSE(broken.is_valid());
    EXPECT_TRUE(broken.find_all("(((").empty());
    const oui::TextSearch empty{""};
    EXPECT_FALSE(empty.is_valid());
    EXPECT_TRUE(empty.find_all("text").empty());
}

TEST(OuiTextSearchTests, ARegularExpressionReadsMatchesBackAsBytesBeyondAscii)
{
    oui::SearchOptions options;
    options.regex = true;
    options.match_case = false;
    // "ñ" is two bytes and "😀" four, as UTF-16 they are one unit and two.
    const oui::TextSearch search{"ca.", options};
    EXPECT_EQ(search.find_all("ñ CAñ 😀 caz"), (Matches{{3, 7}, {13, 16}}));
    // Empty matches are left out, and the search goes on past them.
    const oui::TextSearch maybe{"x*", options};
    EXPECT_EQ(maybe.find_all("ab xx c x"), (Matches{{3, 5}, {8, 9}}));
    // Whole words keep to their edges with an expression too.
    options.whole_word = true;
    const oui::TextSearch words{"TO+DO", options};
    EXPECT_EQ(words.find_all("// TODO todos TOOODO"), (Matches{{3, 7}, {14, 20}}));
}

TEST(OuiTextSearchTests, OneExpressionSearchesFromManyThreadsAtOnce)
{
    oui::SearchOptions options;
    options.regex = true;
    const oui::TextSearch search{R"(\b(TODO|FIXME)\b)", options};
    std::string text;
    for(int line{}; line < 2000; ++ line)
    {
        text += line % 7 == 0 ? "// TODO fix\n" : "plain line\n";
    }
    const auto expected{search.find_all(text).size()};
    ASSERT_GT(expected, 0u);
    std::vector<std::size_t> counts(8);
    std::vector<std::thread> threads;
    for(std::size_t index{}; index < counts.size(); ++ index)
    {
        threads.emplace_back([&search, &text, &counts, index] { counts[index] = search.find_all(text).size(); });
    }
    for(auto& thread : threads)
    {
        thread.join();
    }
    for(const auto count : counts)
    {
        EXPECT_EQ(count, expected);
    }
}

} // namespace
