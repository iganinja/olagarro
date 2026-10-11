#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// How two texts differ, line by line, as a comparison shows them side by side.

// The lines of a text, without their line breaks: "\n" and "\r\n" alike, so two files that differ only in how they
// end their lines read the same. A last line break does not start another line.
[[nodiscard]] std::vector<std::string_view> split_lines(std::string_view text);

// A run of lines that are the same on both sides, only on the left (removed), only on the right (added), or on both
// but different (changed), from the given line of each side, counted from 0.
enum class DiffKind
{
    same,
    removed,
    added,
    changed,
};

struct DiffBlock
{
    DiffKind kind{DiffKind::same};
    std::size_t left_line{};
    std::size_t left_count{};
    std::size_t right_line{};
    std::size_t right_count{};

    friend bool operator==(const DiffBlock& a, const DiffBlock& b)
    {
        return a.kind == b.kind && a.left_line == b.left_line && a.left_count == b.left_count &&
               a.right_line == b.right_line && a.right_count == b.right_count;
    }
};

// The blocks, in order, that take the left lines to the right ones with as few lines removed and added as there can
// be; together they cover every line of both. Texts so different that aligning them would take too long are left as
// one changed block where that is so.
[[nodiscard]] std::vector<DiffBlock> diff_lines(const std::vector<std::string_view>& left,
                                                const std::vector<std::string_view>& right);

// Byte ranges, [first, second), of what changed within a line.
using DiffRanges = std::vector<std::pair<std::size_t, std::size_t>>;

// What changed between two versions of a line, word by word: what is only in the left one, and what only in the
// right one. Spaces and each mark of punctuation count as words of their own.
[[nodiscard]] std::pair<DiffRanges, DiffRanges> diff_within_line(std::string_view left, std::string_view right);

// A row of a comparison side by side: a line on each side, or on one with nothing in front of it. A changed row
// pairs a left line with a right one, and says what changed in each; it says nothing when the two have too little
// in common for that to help, and the whole lines are what changed.
struct DiffRow
{
    DiffKind kind{DiffKind::same};
    std::optional<std::size_t> left_line;
    std::optional<std::size_t> right_line;
    DiffRanges left_changes;
    DiffRanges right_changes;
};

// The rows of a comparison side by side, from the blocks of diff_lines: in a changed block, the first left line
// faces the first right line and so on, and the lines one side has more of face nothing.
[[nodiscard]] std::vector<DiffRow> diff_rows(const std::vector<std::string_view>& left,
                                             const std::vector<std::string_view>& right);

} // namespace olagarro::oui
