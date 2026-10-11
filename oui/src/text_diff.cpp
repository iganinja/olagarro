#include <oui/text_diff.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace olagarro::oui
{

namespace
{

// A run of items kept, removed from the left or added from the right.
enum class Edit
{
    keep,
    remove,
    add,
};

struct Run
{
    Edit edit;
    std::size_t count;
};

// How much looking for the fewest edits may cost, in steps along the diagonals, before the rest is left as
// replaced: some tenths of a second, which two very different texts of tens of thousands of lines would otherwise
// take far longer than.
constexpr std::uint64_t work_limit{200'000'000};

// Myers' difference algorithm, in linear space: the middle of the shortest edit is found from both ends at once,
// and each half is then compared on its own.
class Differ
{
  public:
    Differ(const std::vector<std::uint32_t>& left, const std::vector<std::uint32_t>& right)
        : m_left{left}, m_right{right}
    {
    }

    [[nodiscard]] std::vector<Run> runs()
    {
        compare(0, m_left.size(), 0, m_right.size());
        return std::move(m_runs);
    }

  private:
    void push(Edit edit, std::size_t count)
    {
        if(count == 0)
        {
            return;
        }
        if(!m_runs.empty() && m_runs.back().edit == edit)
        {
            m_runs.back().count += count;
        }
        else
        {
            m_runs.push_back({edit, count});
        }
    }

    void compare(std::size_t left_start, std::size_t left_end, std::size_t right_start, std::size_t right_end)
    {
        std::size_t prefix{};
        while(left_start + prefix < left_end && right_start + prefix < right_end &&
              m_left[left_start + prefix] == m_right[right_start + prefix])
        {
            ++ prefix;
        }
        push(Edit::keep, prefix);
        left_start += prefix;
        right_start += prefix;
        std::size_t suffix{};
        while(left_end - suffix > left_start && right_end - suffix > right_start &&
              m_left[left_end - suffix - 1] == m_right[right_end - suffix - 1])
        {
            ++ suffix;
        }
        left_end -= suffix;
        right_end -= suffix;

        if(left_start == left_end)
        {
            push(Edit::add, right_end - right_start);
        }
        else if(right_start == right_end)
        {
            push(Edit::remove, left_end - left_start);
        }
        else if(const auto middle{share_anything(left_start, left_end, right_start, right_end)
                                      ? bisect(left_start, left_end, right_start, right_end)
                                      : std::nullopt})
        {
            compare(left_start, left_start + middle->first, right_start, right_start + middle->second);
            compare(left_start + middle->first, left_end, right_start + middle->second, right_end);
        }
        else
        {
            push(Edit::remove, left_end - left_start);
            push(Edit::add, right_end - right_start);
        }
        push(Edit::keep, suffix);
    }

    // Whether the two ranges have any item in common: one rewritten whole has none, and is told at once.
    [[nodiscard]] bool share_anything(std::size_t left_start, std::size_t left_end, std::size_t right_start,
                                      std::size_t right_end) const
    {
        const std::unordered_set<std::uint32_t> left(m_left.begin() + static_cast<std::ptrdiff_t>(left_start),
                                                     m_left.begin() + static_cast<std::ptrdiff_t>(left_end));
        return std::any_of(m_right.begin() + static_cast<std::ptrdiff_t>(right_start),
                           m_right.begin() + static_cast<std::ptrdiff_t>(right_end),
                           [&left](std::uint32_t item) { return left.count(item) > 0; });
    }

    // Where the shortest edit between the two ranges crosses its middle, as offsets into each; none once the work
    // allowed is spent.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    bisect(std::size_t left_start, std::size_t left_end, std::size_t right_start, std::size_t right_end)
    {
        const auto* const a{m_left.data() + left_start};
        const auto* const b{m_right.data() + right_start};
        const auto n{static_cast<std::int64_t>(left_end - left_start)};
        const auto m{static_cast<std::int64_t>(right_end - right_start)};
        const auto max_d{(n + m + 1) / 2};
        const auto offset{max_d};
        const auto length{2 * max_d + 2};
        std::vector<std::int64_t> forward(static_cast<std::size_t>(length), -1);
        std::vector<std::int64_t> backward(static_cast<std::size_t>(length), -1);
        forward[static_cast<std::size_t>(offset + 1)] = 0;
        backward[static_cast<std::size_t>(offset + 1)] = 0;
        const auto delta{n - m};
        // With an odd delta the paths from the start meet those from the end on the way forward; with an even
        // one, on the way back.
        const auto front{delta % 2 != 0};
        std::int64_t k1_start{};
        std::int64_t k1_end{};
        std::int64_t k2_start{};
        std::int64_t k2_end{};
        const auto at{[](std::vector<std::int64_t>& v, std::int64_t index) -> std::int64_t&
                      { return v[static_cast<std::size_t>(index)]; }};
        for(std::int64_t d{}; d < max_d; ++ d)
        {
            m_work += static_cast<std::uint64_t>(2 * d + 1);
            if(m_work > work_limit)
            {
                return std::nullopt;
            }
            for(auto k1{-d + k1_start}; k1 <= d - k1_end; k1 += 2)
            {
                const auto k1_offset{offset + k1};
                auto x1{k1 == -d || (k1 != d && at(forward, k1_offset - 1) < at(forward, k1_offset + 1))
                            ? at(forward, k1_offset + 1)
                            : at(forward, k1_offset - 1) + 1};
                auto y1{x1 - k1};
                while(x1 < n && y1 < m && a[x1] == b[y1])
                {
                    ++ x1;
                    ++ y1;
                    ++ m_work;
                }
                at(forward, k1_offset) = x1;
                if(x1 > n)
                {
                    k1_end += 2;
                }
                else if(y1 > m)
                {
                    k1_start += 2;
                }
                else if(front)
                {
                    const auto k2_offset{offset + delta - k1};
                    if(k2_offset >= 0 && k2_offset < length && at(backward, k2_offset) != -1 &&
                       x1 >= n - at(backward, k2_offset))
                    {
                        return std::pair{static_cast<std::size_t>(x1), static_cast<std::size_t>(y1)};
                    }
                }
            }
            for(auto k2{-d + k2_start}; k2 <= d - k2_end; k2 += 2)
            {
                const auto k2_offset{offset + k2};
                auto x2{k2 == -d || (k2 != d && at(backward, k2_offset - 1) < at(backward, k2_offset + 1))
                            ? at(backward, k2_offset + 1)
                            : at(backward, k2_offset - 1) + 1};
                auto y2{x2 - k2};
                while(x2 < n && y2 < m && a[n - x2 - 1] == b[m - y2 - 1])
                {
                    ++ x2;
                    ++ y2;
                    ++ m_work;
                }
                at(backward, k2_offset) = x2;
                if(x2 > n)
                {
                    k2_end += 2;
                }
                else if(y2 > m)
                {
                    k2_start += 2;
                }
                else if(!front)
                {
                    const auto k1_offset{offset + delta - k2};
                    if(k1_offset >= 0 && k1_offset < length && at(forward, k1_offset) != -1)
                    {
                        const auto x1{at(forward, k1_offset)};
                        const auto y1{offset + x1 - k1_offset};
                        if(x1 >= n - x2)
                        {
                            return std::pair{static_cast<std::size_t>(x1), static_cast<std::size_t>(y1)};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    const std::vector<std::uint32_t>& m_left;
    const std::vector<std::uint32_t>& m_right;
    std::vector<Run> m_runs;
    std::uint64_t m_work{};
};

// The items of both sides as numbers, the same number for the same item, so comparing them is comparing numbers.
template <typename Item>
[[nodiscard]] std::pair<std::vector<std::uint32_t>, std::vector<std::uint32_t>> numbered(const std::vector<Item>& left,
                                                                                         const std::vector<Item>& right)
{
    std::unordered_map<std::string_view, std::uint32_t> numbers;
    const auto number{
        [&numbers](const std::vector<Item>& items)
        {
            std::vector<std::uint32_t> found;
            found.reserve(items.size());
            for(const auto& item : items)
            {
                found.push_back(
                    numbers.emplace(std::string_view{item}, static_cast<std::uint32_t>(numbers.size())).first->second);
            }
            return found;
        }};
    auto left_numbers{number(left)};
    auto right_numbers{number(right)};
    return {std::move(left_numbers), std::move(right_numbers)};
}

// A line in words: runs of letters, digits and underscores, with any character beyond ASCII taken for a letter;
// runs of spaces and tabs; and each other character alone.
[[nodiscard]] std::vector<std::string_view> words(std::string_view line)
{
    const auto letter{[](unsigned char character)
                      { return character >= 0x80 || character == '_' || std::isalnum(character) != 0; }};
    const auto space{[](unsigned char character) { return character == ' ' || character == '\t'; }};
    std::vector<std::string_view> found;
    std::size_t index{};
    while(index < line.size())
    {
        const auto first{static_cast<unsigned char>(line[index])};
        auto end{index + 1};
        if(letter(first) || space(first))
        {
            const auto same{letter(first) ? letter : space};
            while(end < line.size() && same(static_cast<unsigned char>(line[end])))
            {
                ++ end;
            }
        }
        found.push_back(line.substr(index, end - index));
        index = end;
    }
    return found;
}

// Whether two versions of a line have enough in common for what changed within them to help: at least this share
// of their bytes kept.
constexpr double least_in_common{0.4};

} // namespace

std::vector<std::string_view> split_lines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t start{};
    while(start < text.size())
    {
        auto end{text.find('\n', start)};
        if(end == std::string_view::npos)
        {
            end = text.size();
        }
        auto line{text.substr(start, end - start)};
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

std::vector<DiffBlock> diff_lines(const std::vector<std::string_view>& left, const std::vector<std::string_view>& right)
{
    const auto [left_numbers, right_numbers]{numbered(left, right)};
    std::vector<DiffBlock> blocks;
    std::size_t left_line{};
    std::size_t right_line{};
    // The edits between two runs kept are one block: removed, added, or both and so changed.
    DiffBlock pending{DiffKind::same, 0, 0, 0, 0};
    const auto flush{[&blocks, &pending, &left_line, &right_line]
                     {
                         if(pending.left_count == 0 && pending.right_count == 0)
                         {
                             return;
                         }
                         pending.kind = pending.right_count == 0  ? DiffKind::removed
                                        : pending.left_count == 0 ? DiffKind::added
                                                                  : DiffKind::changed;
                         blocks.push_back(pending);
                         pending = {DiffKind::same, left_line, 0, right_line, 0};
                     }};
    for(const auto& run : Differ{left_numbers, right_numbers}.runs())
    {
        if(run.edit == Edit::keep)
        {
            flush();
            blocks.push_back({DiffKind::same, left_line, run.count, right_line, run.count});
            left_line += run.count;
            right_line += run.count;
            pending = {DiffKind::same, left_line, 0, right_line, 0};
        }
        else if(run.edit == Edit::remove)
        {
            pending.left_count += run.count;
            left_line += run.count;
        }
        else
        {
            pending.right_count += run.count;
            right_line += run.count;
        }
    }
    flush();
    return blocks;
}

std::pair<DiffRanges, DiffRanges> diff_within_line(std::string_view left, std::string_view right)
{
    const auto left_words{words(left)};
    const auto right_words{words(right)};
    const auto [left_numbers, right_numbers]{numbered(left_words, right_words)};
    DiffRanges removed;
    DiffRanges added;
    // Ranges next to each other are one.
    const auto mark{[](DiffRanges& ranges, std::size_t first, std::size_t second)
                    {
                        if(!ranges.empty() && ranges.back().second == first)
                        {
                            ranges.back().second = second;
                        }
                        else
                        {
                            ranges.emplace_back(first, second);
                        }
                    }};
    std::size_t left_word{};
    std::size_t right_word{};
    std::size_t left_byte{};
    std::size_t right_byte{};
    for(const auto& run : Differ{left_numbers, right_numbers}.runs())
    {
        for(std::size_t count{}; count < run.count; ++ count)
        {
            if(run.edit != Edit::add)
            {
                const auto size{left_words[left_word++].size()};
                if(run.edit == Edit::remove)
                {
                    mark(removed, left_byte, left_byte + size);
                }
                left_byte += size;
            }
            if(run.edit != Edit::remove)
            {
                const auto size{right_words[right_word++].size()};
                if(run.edit == Edit::add)
                {
                    mark(added, right_byte, right_byte + size);
                }
                right_byte += size;
            }
        }
    }
    return {std::move(removed), std::move(added)};
}

std::vector<DiffRow> diff_rows(const std::vector<std::string_view>& left, const std::vector<std::string_view>& right)
{
    const auto bytes{[](const DiffRanges& ranges)
                     {
                         std::size_t total{};
                         for(const auto& [first, second] : ranges)
                         {
                             total += second - first;
                         }
                         return total;
                     }};
    std::vector<DiffRow> rows;
    for(const auto& block : diff_lines(left, right))
    {
        const auto paired{block.kind == DiffKind::same || block.kind == DiffKind::changed
                              ? std::min(block.left_count, block.right_count)
                              : 0};
        for(std::size_t index{}; index < paired; ++ index)
        {
            DiffRow row{block.kind, block.left_line + index, block.right_line + index, {}, {}};
            if(block.kind == DiffKind::changed)
            {
                const auto& before{left[block.left_line + index]};
                const auto& after{right[block.right_line + index]};
                auto [removed, added]{diff_within_line(before, after)};
                const auto kept{before.size() - bytes(removed) + after.size() - bytes(added)};
                if(before.size() + after.size() > 0 &&
                   static_cast<double>(kept) >= least_in_common * static_cast<double>(before.size() + after.size()))
                {
                    row.left_changes = std::move(removed);
                    row.right_changes = std::move(added);
                }
            }
            rows.push_back(std::move(row));
        }
        for(auto index{paired}; index < block.left_count; ++ index)
        {
            rows.push_back({DiffKind::removed, block.left_line + index, std::nullopt, {}, {}});
        }
        for(auto index{paired}; index < block.right_count; ++ index)
        {
            rows.push_back({DiffKind::added, std::nullopt, block.right_line + index, {}, {}});
        }
    }
    return rows;
}

} // namespace olagarro::oui
