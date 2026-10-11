#include <oui/text_diff.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include <string>
#include <string_view>
#include <vector>

using namespace olagarro;

namespace
{

using oui::DiffBlock;
using oui::DiffKind;
using oui::DiffRanges;

[[nodiscard]] std::vector<DiffBlock> diff(std::string_view left, std::string_view right)
{
    return oui::diff_lines(oui::split_lines(left), oui::split_lines(right));
}

// The text of each range of a line, to compare with what is expected.
[[nodiscard]] std::vector<std::string> texts(std::string_view line, const DiffRanges& ranges)
{
    std::vector<std::string> found;
    for(const auto& [first, second] : ranges)
    {
        found.emplace_back(line.substr(first, second - first));
    }
    return found;
}

} // namespace

TEST(OuiTextDiffTests, SplitsLinesWhicheverWayTheyEnd)
{
    EXPECT_EQ(oui::split_lines("a\nb\n"), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(oui::split_lines("a\r\nb"), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(oui::split_lines("a\n\nb\n"), (std::vector<std::string_view>{"a", "", "b"}));
    EXPECT_TRUE(oui::split_lines("").empty());
}

TEST(OuiTextDiffTests, TheSameTextIsOneBlockTheSame)
{
    EXPECT_EQ(diff("a\nb\nc\n", "a\nb\nc\n"), (std::vector<DiffBlock>{{DiffKind::same, 0, 3, 0, 3}}));
    EXPECT_TRUE(diff("", "").empty());
}

TEST(OuiTextDiffTests, FindsALineAddedRemovedOrChanged)
{
    EXPECT_EQ(diff("a\nc\n", "a\nb\nc\n"),
              (std::vector<DiffBlock>{
                  {DiffKind::same, 0, 1, 0, 1}, {DiffKind::added, 1, 0, 1, 1}, {DiffKind::same, 1, 1, 2, 1}}));
    EXPECT_EQ(diff("a\nb\nc\n", "a\nc\n"),
              (std::vector<DiffBlock>{
                  {DiffKind::same, 0, 1, 0, 1}, {DiffKind::removed, 1, 1, 1, 0}, {DiffKind::same, 2, 1, 1, 1}}));
    EXPECT_EQ(diff("a\nb\nc\n", "a\nB\nc\n"),
              (std::vector<DiffBlock>{
                  {DiffKind::same, 0, 1, 0, 1}, {DiffKind::changed, 1, 1, 1, 1}, {DiffKind::same, 2, 1, 2, 1}}));
}

TEST(OuiTextDiffTests, EverythingAgainstNothing)
{
    EXPECT_EQ(diff("", "a\nb\n"), (std::vector<DiffBlock>{{DiffKind::added, 0, 0, 0, 2}}));
    EXPECT_EQ(diff("a\nb\n", ""), (std::vector<DiffBlock>{{DiffKind::removed, 0, 2, 0, 0}}));
}

TEST(OuiTextDiffTests, KeepsAsManyLinesAsThereAreInCommon)
{
    // The longest run in common is b c d e: one line out at the start, one in at the end.
    EXPECT_EQ(diff("a\nb\nc\nd\ne\n", "b\nc\nd\ne\nf\n"),
              (std::vector<DiffBlock>{
                  {DiffKind::removed, 0, 1, 0, 0}, {DiffKind::same, 1, 4, 0, 4}, {DiffKind::added, 5, 0, 4, 1}}));
}

TEST(OuiTextDiffTests, TheBlocksCoverEveryLineOfBothInOrder)
{
    // Text that moves about: each block starts where the one before it ended, on both sides.
    std::string left;
    std::string right;
    for(int line{}; line < 400; ++ line)
    {
        left += "line " + std::to_string(line % 37) + "\n";
        right += "line " + std::to_string((line * 7) % 41) + "\n";
    }
    const auto blocks{diff(left, right)};
    std::size_t left_line{};
    std::size_t right_line{};
    std::size_t same{};
    for(const auto& block : blocks)
    {
        EXPECT_EQ(block.left_line, left_line);
        EXPECT_EQ(block.right_line, right_line);
        if(block.kind == DiffKind::same)
        {
            EXPECT_EQ(block.left_count, block.right_count);
            same += block.left_count;
        }
        left_line += block.left_count;
        right_line += block.right_count;
    }
    EXPECT_EQ(left_line, 400u);
    EXPECT_EQ(right_line, 400u);
    EXPECT_GT(same, 0u);
}

TEST(OuiTextDiffTests, TwoLargeTextsWithNothingInCommonEndQuickly)
{
    std::string left;
    std::string right;
    for(int line{}; line < 20000; ++ line)
    {
        left += "left " + std::to_string(line) + "\n";
        right += "right " + std::to_string(line) + "\n";
    }
    EXPECT_EQ(diff(left, right), (std::vector<DiffBlock>{{DiffKind::changed, 0, 20000, 0, 20000}}));
}

TEST(OuiTextDiffTests, WithinALineFindsTheWordsThatChanged)
{
    const std::string_view left{"int count = total + 1;"};
    const std::string_view right{"int count = sum + 1;"};
    const auto [removed, added]{oui::diff_within_line(left, right)};
    EXPECT_EQ(texts(left, removed), (std::vector<std::string>{"total"}));
    EXPECT_EQ(texts(right, added), (std::vector<std::string>{"sum"}));
}

TEST(OuiTextDiffTests, WithinALineWordsAddedAndRemovedNextToEachOtherAreOneRange)
{
    const std::string_view left{"call(a, b)"};
    const std::string_view right{"call(a, b, c, d)"};
    const auto [removed, added]{oui::diff_within_line(left, right)};
    EXPECT_TRUE(removed.empty());
    EXPECT_EQ(texts(right, added), (std::vector<std::string>{", c, d"}));
}

TEST(OuiTextDiffTests, WithinALineLettersBeyondAsciiArePartOfTheirWord)
{
    const std::string_view left{"año viejo"};
    const std::string_view right{"año nuevo"};
    const auto [removed, added]{oui::diff_within_line(left, right)};
    EXPECT_EQ(texts(left, removed), (std::vector<std::string>{"viejo"}));
    EXPECT_EQ(texts(right, added), (std::vector<std::string>{"nuevo"}));
}

TEST(OuiTextDiffTests, RowsPairTheLinesOfAChangeAndLeaveTheRestFacingNothing)
{
    const auto left{oui::split_lines("a\nx = 1\ny = 2\nb\n")};
    const auto right{oui::split_lines("a\nx = 3\nb\n")};
    const auto rows{oui::diff_rows(left, right)};
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].kind, DiffKind::same);
    EXPECT_EQ(rows[1].kind, DiffKind::changed);
    EXPECT_EQ(rows[1].left_line, std::optional<std::size_t>{1});
    EXPECT_EQ(rows[1].right_line, std::optional<std::size_t>{1});
    EXPECT_EQ(texts(left[1], rows[1].left_changes), (std::vector<std::string>{"1"}));
    EXPECT_EQ(texts(right[1], rows[1].right_changes), (std::vector<std::string>{"3"}));
    EXPECT_EQ(rows[2].kind, DiffKind::removed);
    EXPECT_EQ(rows[2].left_line, std::optional<std::size_t>{2});
    EXPECT_FALSE(rows[2].right_line);
    EXPECT_EQ(rows[3].kind, DiffKind::same);
    EXPECT_EQ(rows[3].left_line, std::optional<std::size_t>{3});
    EXPECT_EQ(rows[3].right_line, std::optional<std::size_t>{2});
}

TEST(OuiTextDiffTests, RowsOfLinesWithLittleInCommonSayNothingWithin)
{
    const auto left{oui::split_lines("return first_value;\n")};
    const auto right{oui::split_lines("throw std::runtime_error{message};\n")};
    const auto rows{oui::diff_rows(left, right)};
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].kind, DiffKind::changed);
    EXPECT_TRUE(rows[0].left_changes.empty());
    EXPECT_TRUE(rows[0].right_changes.empty());
}

TEST(OuiTextDiffTests, KeepsTheLongestRunInCommonThatThereIs)
{
    // Small texts of few different lines, many times over, against the longest common subsequence counted the slow
    // way: the lines kept the same are always as many as can be.
    std::uint32_t seed{12345};
    const auto random{[&seed](std::uint32_t below)
                      {
                          seed = seed * 1664525u + 1013904223u;
                          return (seed >> 16) % below;
                      }};
    for(int round{}; round < 500; ++ round)
    {
        std::string left;
        std::string right;
        const auto left_size{random(12)};
        const auto right_size{random(12)};
        for(std::uint32_t line{}; line < left_size; ++ line)
        {
            left += std::string(1, static_cast<char>('a' + random(4))) + "\n";
        }
        for(std::uint32_t line{}; line < right_size; ++ line)
        {
            right += std::string(1, static_cast<char>('a' + random(4))) + "\n";
        }
        const auto a{oui::split_lines(left)};
        const auto b{oui::split_lines(right)};
        std::vector<std::vector<std::size_t>> longest(a.size() + 1, std::vector<std::size_t>(b.size() + 1));
        for(std::size_t i{1}; i <= a.size(); ++ i)
        {
            for(std::size_t j{1}; j <= b.size(); ++ j)
            {
                longest[i][j] =
                    a[i - 1] == b[j - 1] ? longest[i - 1][j - 1] + 1 : std::max(longest[i - 1][j], longest[i][j - 1]);
            }
        }
        std::size_t same{};
        for(const auto& block : oui::diff_lines(a, b))
        {
            if(block.kind == DiffKind::same)
            {
                same += block.left_count;
                for(std::size_t line{}; line < block.left_count; ++ line)
                {
                    EXPECT_EQ(a[block.left_line + line], b[block.right_line + line]);
                }
            }
        }
        EXPECT_EQ(same, longest[a.size()][b.size()]) << left << "--\n" << right;
    }
}

TEST(OuiTextDiffTests, ALargeTextWithManySmallChangesIsAlignedExactly)
{
    std::string left;
    std::string right;
    for(int line{}; line < 50000; ++ line)
    {
        left += "line " + std::to_string(line) + "\n";
        right += (line % 25 == 0 ? "changed " : "line ") + std::to_string(line) + "\n";
    }
    std::size_t changed{};
    for(const auto& block : diff(left, right))
    {
        EXPECT_NE(block.kind, DiffKind::removed);
        EXPECT_NE(block.kind, DiffKind::added);
        if(block.kind == DiffKind::changed)
        {
            EXPECT_EQ(block.left_count, 1u);
            ++ changed;
        }
    }
    EXPECT_EQ(changed, 2000u);
}
