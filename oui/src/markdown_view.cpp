#include <oui/markdown_view.h>

#include <oui/container.h>
#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>
#include <oui/utf8.h>

#include <algorithm>
#include <map>
#include <cctype>
#include <numeric>
#include <optional>

namespace olagarro::oui
{

namespace
{

using Role = MarkdownView::Role;
using Run = MarkdownView::Run;
using Line = MarkdownView::Line;

[[nodiscard]] bool is_word_character(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || static_cast<unsigned char>(c) >= 0x80;
}

[[nodiscard]] std::string_view trim_left(std::string_view text)
{
    const auto first{text.find_first_not_of(" \t")};
    return first == std::string_view::npos ? std::string_view{} : text.substr(first);
}

[[nodiscard]] std::string_view trim(std::string_view text)
{
    text = trim_left(text);
    const auto last{text.find_last_not_of(" \t")};
    return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
}

[[nodiscard]] std::string repeat(const std::string_view piece, const int times)
{
    std::string result;
    for(auto count{0}; count < times; ++ count)
    {
        result += piece;
    }
    return result;
}

[[nodiscard]] std::string expand_tabs(const std::string_view text)
{
    std::string result;
    for(const auto c : text)
    {
        if(c == '\t')
        {
            result.append(4 - result.size() % 4, ' ');
        }
        else
        {
            result += c;
        }
    }
    return result;
}

void append(std::vector<Run>& runs, const std::string_view text, const Role role, const std::string_view target = {},
            const int link = -1)
{
    if(text.empty())
    {
        return;
    }
    // Two links side by side stay two.
    if(!runs.empty() && runs.back().role == role && runs.back().link == link)
    {
        runs.back().text += text;
        return;
    }
    runs.emplace_back(std::string{text}, role);
    runs.back().target = target;
    runs.back().link = link;
}

// A heading's name as a link to its section says it, as GitHub names them: letters in lower case, spaces as
// hyphens, and of the rest of ASCII only digits, hyphens and underscores; letters beyond ASCII as they are.
[[nodiscard]] std::string anchor_of(const std::vector<Run>& runs)
{
    std::string anchor;
    for(const auto& run : runs)
    {
        for(const auto c : run.text)
        {
            const auto byte{static_cast<unsigned char>(c)};
            if(byte >= 0x80 || std::isalnum(byte) != 0 || c == '-' || c == '_')
            {
                anchor += byte < 0x80 ? static_cast<char>(std::tolower(byte)) : c;
            }
            else if(c == ' ')
            {
                anchor += '-';
            }
        }
    }
    return anchor;
}

// The inline formatting of `text`: `code`, **strong**, *emphasis*, [links](url) and backslash escapes. Plain
// text takes `base`. Underscores only format at word edges, so snake_case names stay as they are. Links are
// numbered from `links` on, which counts them.
// Text to show as prose: a gap of blanks with a tab in it is one space, as a browser shows it, since a font has no
// tab to draw.
[[nodiscard]] std::string tabs_as_spaces(const std::string_view text)
{
    std::string result;
    for(std::size_t index{}; index < text.size();)
    {
        if(text[index] != ' ' && text[index] != '\t')
        {
            result += text[index++];
            continue;
        }
        const auto end{std::min(text.find_first_not_of(" \t", index), text.size())};
        const auto gap{text.substr(index, end - index)};
        result += gap.find('\t') == std::string_view::npos ? std::string{gap} : std::string{" "};
        index = end;
    }
    return result;
}

[[nodiscard]] std::vector<Run> inline_runs(const std::string_view source, const Role base, int& links)
{
    const auto prose{tabs_as_spaces(source)};
    const std::string_view text{prose};
    std::vector<Run> runs;
    std::size_t index{};
    while(index < text.size())
    {
        const auto c{text[index]};
        if(c == '\\' && index + 1 < text.size() && std::ispunct(static_cast<unsigned char>(text[index + 1])) != 0)
        {
            append(runs, text.substr(index + 1, 1), base);
            index += 2;
            continue;
        }
        if(c == '`')
        {
            const auto close{text.find('`', index + 1)};
            if(close != std::string_view::npos)
            {
                append(runs, text.substr(index + 1, close - index - 1), Role::code);
                index = close + 1;
                continue;
            }
        }
        if(c == '*' || c == '_')
        {
            auto length{std::size_t{1}};
            while(index + length < text.size() && text[index + length] == c && length < 3)
            {
                ++ length;
            }
            const auto opens{c == '*' || index == 0 || !is_word_character(text[index - 1])};
            const std::string delimiter(length, c);
            auto close{opens ? text.find(delimiter, index + length) : std::string_view::npos};
            // An underscore closes only where the word ends.
            while(c == '_' && close != std::string_view::npos && close + length < text.size() &&
                  is_word_character(text[close + length]))
            {
                close = text.find(delimiter, close + 1);
            }
            if(close != std::string_view::npos && close > index + length)
            {
                append(runs, text.substr(index + length, close - index - length),
                       base == Role::text ? (length >= 2 ? Role::strong : Role::emphasis) : base);
                index = close + length;
                continue;
            }
            append(runs, delimiter, base);
            index += length;
            continue;
        }
        if(c == '[')
        {
            const auto close{text.find(']', index + 1)};
            if(close != std::string_view::npos && close + 1 < text.size() && text[close + 1] == '(')
            {
                const auto end{text.find(')', close + 2)};
                if(end != std::string_view::npos)
                {
                    append(runs, text.substr(index + 1, close - index - 1), Role::link,
                           text.substr(close + 2, end - close - 2), links++);
                    index = end + 1;
                    continue;
                }
            }
        }
        append(runs, text.substr(index, 1), base);
        ++ index;
    }
    return runs;
}

[[nodiscard]] int line_width(const Line& line)
{
    auto width{0};
    for(const auto& run : line)
    {
        width += text_width(run.text);
    }
    return width;
}

// How wide a line of prose is as it is drawn, in the units text_metrics() measures: its text as the interface font
// does, which in a window is narrower than the cells, and the spaces it starts with in cells, as they are drawn.
[[nodiscard]] int drawn_width(const Line& line)
{
    const auto& metrics{text_metrics()};
    auto width{0};
    auto leading{true};
    for(const auto& run : line)
    {
        leading = leading && run.text.find_first_not_of(' ') == std::string::npos;
        width += leading ? text_width(run.text) * metrics.cell_width() : metrics.width(run.text);
    }
    return width;
}

// Splits a run at `columns` terminal columns, for what keeps to the grid, as a table's cells: the part that fits, and
// the rest.
[[nodiscard]] std::pair<Run, Run> split_at_columns(const Run& run, const int columns)
{
    auto used{0};
    std::size_t index{};
    while(index < run.text.size())
    {
        auto next{index};
        const auto glyph{decode_utf8(run.text, next)};
        if(used + terminal_cell_width(glyph) > columns)
        {
            break;
        }
        used += terminal_cell_width(glyph);
        index = next;
    }
    auto head{run};
    head.text = run.text.substr(0, index);
    auto tail{run};
    tail.text = run.text.substr(index);
    tail.anchor.clear();
    return {std::move(head), std::move(tail)};
}

// Splits a run of prose where `room` ends, in the units text_metrics() measures: the part that fits, and the rest.
[[nodiscard]] std::pair<Run, Run> split_run(const Run& run, const int room)
{
    const auto& metrics{text_metrics()};
    std::size_t index{};
    while(index < run.text.size())
    {
        auto next{index};
        static_cast<void>(decode_utf8(run.text, next));
        if(metrics.width(std::string_view{run.text}.substr(0, next)) > room)
        {
            break;
        }
        index = next;
    }
    return {{run.text.substr(0, index), run.role}, {run.text.substr(index), run.role}};
}

// Lays `runs` out as words over lines of `width` columns: the first line starts with `first`, the others with
// `rest`. A word wider than a whole line breaks where the line ends.
void wrap_runs(const std::vector<Run>& runs, const int width, const Line& first, const Line& rest,
               std::vector<Line>& out)
{
    // Words, each made of the runs between spaces, so a word can mix styles, as in "**bold**,".
    std::vector<Line> words(1);
    for(const auto& run : runs)
    {
        std::size_t start{};
        while(start <= run.text.size())
        {
            const auto space{run.text.find(' ', start)};
            const auto end{space == std::string::npos ? run.text.size() : space};
            if(end > start)
            {
                // A piece of the run, a link's or a heading's still.
                auto piece{run};
                piece.text = run.text.substr(start, end - start);
                if(start > 0)
                {
                    piece.anchor.clear();
                }
                words.back().push_back(std::move(piece));
            }
            if(space == std::string::npos)
            {
                break;
            }
            if(!words.back().empty())
            {
                words.emplace_back();
            }
            start = space + 1;
        }
    }
    auto line{first};
    auto line_has_words{false};
    for(auto& word : words)
    {
        if(word.empty())
        {
            continue;
        }
        // Whether the word fits is measured as it is drawn, so a line of prose reaches the edge in any font.
        const auto& metrics{text_metrics()};
        const auto room{width * metrics.cell_width()};
        if(line_has_words && drawn_width(line) + metrics.width(" ") + drawn_width(word) > room)
        {
            out.push_back(std::move(line));
            line = rest;
            line_has_words = false;
        }
        if(line_has_words)
        {
            line.push_back({" ", Role::text});
        }
        for(auto& piece : word)
        {
            // A piece too wide for what is left of a line breaks over as many lines as it takes.
            auto remaining{piece};
            while(drawn_width(line) + metrics.width(remaining.text) > room && drawn_width(line) < room)
            {
                auto [head, tail]{split_run(remaining, room - drawn_width(line))};
                if(head.text.empty())
                {
                    break;
                }
                line.push_back(std::move(head));
                out.push_back(std::move(line));
                line = rest;
                remaining = std::move(tail);
            }
            if(!remaining.text.empty())
            {
                line.push_back(std::move(remaining));
            }
        }
        line_has_words = true;
    }
    if(line_has_words || line.size() > first.size())
    {
        out.push_back(std::move(line));
    }
}

[[nodiscard]] std::optional<int> heading_level(const std::string_view line)
{
    auto level{0};
    while(level < static_cast<int>(line.size()) && line[static_cast<std::size_t>(level)] == '#')
    {
        ++ level;
    }
    if(level == 0 || level > 6 || static_cast<std::size_t>(level) >= line.size() ||
       line[static_cast<std::size_t>(level)] != ' ')
    {
        return std::nullopt;
    }
    return level;
}

[[nodiscard]] bool is_rule(const std::string_view line)
{
    const auto text{trim(line)};
    if(text.size() < 3 || (text[0] != '-' && text[0] != '*' && text[0] != '_'))
    {
        return false;
    }
    auto count{0};
    for(const auto c : text)
    {
        if(c == text[0])
        {
            ++ count;
        }
        else if(c != ' ')
        {
            return false;
        }
    }
    return count >= 3;
}

struct ListItem
{
    int indent;
    std::string marker;
    std::string_view text;
};

[[nodiscard]] std::optional<ListItem> list_item(const std::string_view line)
{
    const auto text{trim_left(line)};
    const auto indent{static_cast<int>(line.size() - text.size())};
    if(text.size() >= 2 && (text[0] == '-' || text[0] == '*' || text[0] == '+') && text[1] == ' ')
    {
        return ListItem{indent, "•", trim_left(text.substr(2))};
    }
    std::size_t digits{};
    while(digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])) != 0)
    {
        ++ digits;
    }
    if(digits > 0 && digits + 1 < text.size() && (text[digits] == '.' || text[digits] == ')') &&
       text[digits + 1] == ' ')
    {
        return ListItem{indent, std::string{text.substr(0, digits + 1)}, trim_left(text.substr(digits + 2))};
    }
    return std::nullopt;
}

[[nodiscard]] bool is_table_separator(const std::string_view line)
{
    const auto text{trim(line)};
    return text.size() >= 3 && text[0] == '|' && text.find('-') != std::string_view::npos &&
           text.find_first_not_of("|-: ") == std::string_view::npos;
}

[[nodiscard]] std::vector<std::string_view> table_cells(std::string_view line)
{
    line = trim(line);
    if(!line.empty() && line.front() == '|')
    {
        line.remove_prefix(1);
    }
    if(!line.empty() && line.back() == '|')
    {
        line.remove_suffix(1);
    }
    std::vector<std::string_view> cells;
    std::size_t start{};
    for(std::size_t index{}; index <= line.size(); ++ index)
    {
        if(index == line.size() || (line[index] == '|' && (index == 0 || line[index - 1] != '\\')))
        {
            cells.push_back(trim(line.substr(start, index - start)));
            start = index + 1;
        }
    }
    return cells;
}

// A line that starts a block of its own, which ends a paragraph or a list item before it.
[[nodiscard]] bool starts_block(const std::string_view line)
{
    const auto text{trim_left(line)};
    return text.empty() || heading_level(text) || text.rfind("```", 0) == 0 || text.front() == '>' ||
           text.front() == '|' || is_rule(text) || list_item(line);
}

void layout_table(const std::vector<std::string_view>& rows, const int width, int& links, std::vector<Line>& out)
{
    std::vector<std::vector<std::vector<Run>>> cells;
    std::size_t columns{};
    for(std::size_t row{}; row < rows.size(); ++ row)
    {
        std::vector<std::vector<Run>> parsed;
        for(const auto cell : table_cells(rows[row]))
        {
            parsed.push_back(inline_runs(cell, row == 0 ? Role::table_header : Role::text, links));
        }
        columns = std::max(columns, parsed.size());
        cells.push_back(std::move(parsed));
    }
    const std::string separator{" │ "};
    std::vector<int> widths(columns, 1);
    for(const auto& row : cells)
    {
        for(std::size_t column{}; column < row.size(); ++ column)
        {
            widths[column] = std::max(widths[column], line_width(row[column]));
        }
    }
    // Too wide: the widest column gives way first, down to a few columns each.
    const auto total{[&widths, &separator]
                     {
                         return std::accumulate(widths.begin(), widths.end(), 0) +
                                text_width(separator) * static_cast<int>(widths.empty() ? 0 : widths.size() - 1);
                     }};
    while(total() > width)
    {
        const auto widest{std::max_element(widths.begin(), widths.end())};
        if(*widest <= 3)
        {
            break;
        }
        --*widest;
    }
    for(std::size_t row{}; row < cells.size(); ++ row)
    {
        Line line;
        for(std::size_t column{}; column < columns; ++ column)
        {
            if(column > 0)
            {
                line.push_back({separator, Role::marker});
            }
            const auto cell{column < cells[row].size() ? cells[row][column] : std::vector<Run>{}};
            auto used{0};
            for(const auto& run : cell)
            {
                const auto room{widths[column] - used};
                if(room <= 0)
                {
                    break;
                }
                if(text_width(run.text) > room)
                {
                    auto head{split_at_columns(run, room - 1).first};
                    head.text += "~";
                    used += text_width(head.text);
                    line.push_back(std::move(head));
                    break;
                }
                used += text_width(run.text);
                line.push_back(run);
            }
            line.push_back(
                {std::string(static_cast<std::size_t>(std::max(0, widths[column] - used)), ' '), Role::text});
        }
        out.push_back(std::move(line));
        if(row == 0)
        {
            Line rule;
            for(std::size_t column{}; column < columns; ++ column)
            {
                rule.push_back({(column > 0 ? "─┼─" : "") + repeat("─", widths[column]), Role::rule});
            }
            out.push_back(std::move(rule));
        }
    }
}

// Whether a run keeps its line to the grid of cells, as tables, rules and code do, so their columns line up.
[[nodiscard]] bool on_the_grid(const Run& run)
{
    return run.role == Role::code_block || run.role == Role::rule || run.role == Role::table_header ||
           run.text == " │ ";
}

} // namespace

void MarkdownView::set_markdown(std::string markdown)
{
    m_chosen_link.reset();
    // The reading place stays, so a view that follows a text being written keeps still.
    m_markdown = std::move(markdown);
    m_laid_out_width = -1;
    if(bounds().width > 0)
    {
        arrange(bounds());
    }
}

const std::string& MarkdownView::markdown() const
{
    return m_markdown;
}

std::vector<MarkdownView::Line> MarkdownView::layout(const int width) const
{
    // How many headings had each name so far.
    std::map<std::string, int> anchors_seen;
    std::vector<std::string_view> lines;
    for(std::size_t start{}; start <= m_markdown.size();)
    {
        auto end{m_markdown.find('\n', start)};
        if(end == std::string::npos)
        {
            end = m_markdown.size();
        }
        auto line{std::string_view{m_markdown}.substr(start, end - start)};
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        start = end + 1;
    }
    std::vector<Line> out;
    // The links, numbered in the order they come.
    auto links{0};
    // Blocks stand one blank line apart, except the items of one list.
    auto previous_was_list{false};
    const auto begin_block{[&out, &previous_was_list](const bool list)
                           {
                               if(!out.empty() && !(list && previous_was_list))
                               {
                                   out.emplace_back();
                               }
                               previous_was_list = list;
                           }};
    std::size_t index{};
    while(index < lines.size())
    {
        const auto line{lines[index]};
        const auto text{trim_left(line)};
        if(text.empty())
        {
            ++ index;
            continue;
        }
        if(text.rfind("```", 0) == 0)
        {
            begin_block(false);
            for(++ index; index < lines.size() && trim_left(lines[index]).rfind("```", 0) != 0; ++ index)
            {
                out.push_back({{expand_tabs(lines[index]), Role::code_block}});
            }
            ++ index;
            continue;
        }
        if(const auto level{heading_level(text)})
        {
            begin_block(false);
            const auto title{trim(text.substr(static_cast<std::size_t>(*level) + 1))};
            auto runs{inline_runs(title, Role::heading, links)};
            const auto first_line{out.size()};
            // A name another heading has already gets "-1", "-2"... after it, as GitHub names them.
            auto anchor{anchor_of(runs)};
            if(const auto times{anchors_seen[anchor]++}; times > 0)
            {
                anchor += "-" + std::to_string(times);
            }
            wrap_runs(runs, width, {}, {}, out);
            if(first_line < out.size() && !out[first_line].empty())
            {
                out[first_line].front().anchor = anchor;
            }
            if(*level == 1)
            {
                out.push_back({{repeat("═", std::min(width, text_width(title))), Role::heading}});
            }
            ++ index;
            continue;
        }
        if(is_rule(text))
        {
            begin_block(false);
            out.push_back({{repeat("─", width), Role::rule}});
            ++ index;
            continue;
        }
        if(text.front() == '|' && index + 1 < lines.size() && is_table_separator(lines[index + 1]))
        {
            begin_block(false);
            std::vector<std::string_view> rows{line};
            for(index += 2; index < lines.size() && trim_left(lines[index]).rfind('|', 0) == 0; ++ index)
            {
                rows.push_back(lines[index]);
            }
            layout_table(rows, width, links, out);
            continue;
        }
        if(text.front() == '>')
        {
            begin_block(false);
            std::string quote;
            for(; index < lines.size() && trim_left(lines[index]).rfind('>', 0) == 0; ++ index)
            {
                const auto content{trim(trim_left(lines[index]).substr(1))};
                quote += (quote.empty() ? "" : " ") + std::string{content};
            }
            const Line bar{{"│ ", Role::marker}};
            wrap_runs(inline_runs(quote, Role::emphasis, links), width, bar, bar, out);
            continue;
        }
        if(const auto item{list_item(line)})
        {
            begin_block(true);
            std::string content{item->text};
            for(++ index; index < lines.size() && !starts_block(lines[index]); ++ index)
            {
                content += " " + std::string{trim(lines[index])};
            }
            const auto indent{std::string(static_cast<std::size_t>(std::min(item->indent, 12)), ' ')};
            const Line first{{indent, Role::text}, {item->marker + " ", Role::marker}};
            const Line rest{{std::string(static_cast<std::size_t>(line_width(first)), ' '), Role::text}};
            wrap_runs(inline_runs(content, Role::text, links), width, first, rest, out);
            continue;
        }
        begin_block(false);
        std::string paragraph{trim(line)};
        for(++ index; index < lines.size() && !starts_block(lines[index]); ++ index)
        {
            paragraph += " " + std::string{trim(lines[index])};
        }
        wrap_runs(inline_runs(paragraph, Role::text, links), width, {}, {}, out);
    }
    return out;
}

void MarkdownView::set_background(const ColorRef background)
{
    m_background = background;
}

void MarkdownView::set_foreground(const ColorRef foreground)
{
    m_foreground = foreground;
}

int MarkdownView::scroll_offset() const
{
    return m_scroll;
}

void MarkdownView::scroll_to(const int line)
{
    m_scroll = std::clamp(line, 0, max_scroll());
}

int MarkdownView::text_width_available() const
{
    // A column of margin on the left and one for the scrollbar on the right.
    return std::max(1, whole(bounds().width) - 2);
}

int MarkdownView::max_scroll() const
{
    return std::max(0, static_cast<int>(m_lines.size()) - whole(bounds().height));
}

bool MarkdownView::is_focusable() const
{
    return true;
}

bool MarkdownView::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)}; mouse != nullptr && mouse->wheel_delta != 0)
    {
        scroll_to(m_scroll - mouse->wheel_delta * 3);
        return true;
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)};
       mouse != nullptr && static_cast<int>(m_lines.size()) > bounds().height)
    {
        if(const auto to{m_scrollbar.handle(*mouse, {bounds().x + bounds().width - 1, bounds().y, 1, bounds().height},
                                            whole(bounds().height), static_cast<int>(m_lines.size()), m_scroll,
                                            max_scroll(), std::max(1, whole(bounds().height) - 1))})
        {
            scroll_to(*to);
            return true;
        }
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)};
       mouse != nullptr && mouse->pressed && !mouse->hover && !mouse->dragging && !mouse->right)
    {
        if(const auto link{link_at(mouse->x, mouse->y, mouse->cell_offset)})
        {
            m_chosen_link = link;
            follow(*link);
            return true;
        }
        return false;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    // With Ctrl or Alt the keys are someone else's, such as the tabs the view is in.
    if(key == nullptr || key->control || key->alt)
    {
        return false;
    }
    const auto page{std::max(1, whole(bounds().height) - 1)};
    switch(key->key)
    {
    // From link to link; past the last, or before the first, Tab is left unused, for the next field.
    case Key::tab:
    case Key::shift_tab:
    {
        auto links{0};
        for(const auto& line : m_lines)
        {
            for(const auto& run : line)
            {
                links = std::max(links, run.link + 1);
            }
        }
        const auto next{key->key == Key::tab ? (m_chosen_link ? *m_chosen_link + 1 : 0)
                                             : (m_chosen_link ? *m_chosen_link - 1 : links - 1)};
        if(next < 0 || next >= links)
        {
            m_chosen_link.reset();
            return false;
        }
        m_chosen_link = next;
        if(const auto line{line_of_link(next)}; line && (*line < m_scroll || *line >= m_scroll + bounds().height))
        {
            scroll_to(*line - whole(bounds().height) / 2);
        }
        return true;
    }
    case Key::enter:
        if(!m_chosen_link)
        {
            return false;
        }
        follow(*m_chosen_link);
        return true;
    // An arrow with nowhere to go is left unused, so a dialog can step to its next field.
    case Key::up:
        if(m_scroll == 0)
        {
            return false;
        }
        scroll_to(m_scroll - 1);
        return true;
    case Key::down:
        if(m_scroll >= max_scroll())
        {
            return false;
        }
        scroll_to(m_scroll + 1);
        return true;
    case Key::page_up:
        scroll_to(m_scroll - page);
        return true;
    case Key::page_down:
        scroll_to(m_scroll + page);
        return true;
    case Key::home:
        scroll_to(0);
        return true;
    case Key::end:
        scroll_to(max_scroll());
        return true;
    default:
        return false;
    }
}

bool MarkdownView::scroll_to_anchor(std::string_view anchor)
{
    for(std::size_t index{}; index < m_lines.size(); ++ index)
    {
        if(!m_lines[index].empty() && m_lines[index].front().anchor == anchor)
        {
            scroll_to(static_cast<int>(index));
            return true;
        }
    }
    return false;
}

std::optional<int> MarkdownView::line_of_link(int link) const
{
    for(std::size_t index{}; index < m_lines.size(); ++ index)
    {
        for(const auto& run : m_lines[index])
        {
            if(run.link == link)
            {
                return static_cast<int>(index);
            }
        }
    }
    return std::nullopt;
}

void MarkdownView::follow(int link)
{
    const auto line{line_of_link(link)};
    if(!line)
    {
        return;
    }
    for(const auto& run : m_lines[static_cast<std::size_t>(*line)])
    {
        if(run.link != link)
        {
            continue;
        }
        if(!run.target.empty() && run.target.front() == '#')
        {
            static_cast<void>(scroll_to_anchor(std::string_view{run.target}.substr(1)));
            return;
        }
        m_link.fire(run.target);
        return;
    }
}

std::optional<int> MarkdownView::link_at(double x, double y, int offset) const
{
    const auto row{m_scroll + whole(y - bounds().y)};
    if(row < 0 || row >= static_cast<int>(m_lines.size()) || y >= bounds().y + bounds().height)
    {
        return std::nullopt;
    }
    // Where the pointer is along the line, and where each run is, in the units the line is drawn in, as paint does.
    const auto& metrics{text_metrics()};
    const auto cell{std::max(1, metrics.cell_width())};
    const auto at{(whole(x - bounds().x) - 1) * cell + offset};
    const auto& line{m_lines[static_cast<std::size_t>(row)]};
    const auto on_grid{std::any_of(line.begin(), line.end(), [](const Run& run) { return on_the_grid(run); })};
    auto start{0};
    auto prose{false};
    for(const auto& run : line)
    {
        const auto in_cells{on_grid || (!prose && run.text.find_first_not_of(' ') == std::string::npos)};
        prose = prose || !in_cells;
        const auto width{in_cells ? text_width(run.text) * cell : metrics.width(run.text)};
        if(at >= start && at < start + width)
        {
            return run.link >= 0 ? std::optional<int>{run.link} : std::nullopt;
        }
        start += width;
    }
    return std::nullopt;
}

Size MarkdownView::measure(const Constraints& constraints) const
{
    const auto lines{layout(std::max(1, whole(constraints.max_width) - 2))};
    auto width{0};
    for(const auto& line : lines)
    {
        width = std::max(width, line_width(line) + 2);
    }
    return constraints.constrain({static_cast<double>(width), static_cast<double>(lines.size())});
}

void MarkdownView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(m_laid_out_width != text_width_available())
    {
        m_laid_out_width = text_width_available();
        m_lines = layout(m_laid_out_width);
    }
    m_scroll = std::clamp(m_scroll, 0, max_scroll());
}

TextStyle MarkdownView::run_style(Role role, const Theme& theme, Color background) const
{
    TextStyle style{resolve(m_foreground, theme, theme.editor_foreground), background};
    switch(role)
    {
    case Role::text:
        break;
    case Role::strong:
        style.attributes |= text_bold;
        break;
    case Role::emphasis:
        style.attributes |= text_italic;
        break;
    case Role::code:
    case Role::code_block:
        style = {theme.code_foreground, theme.code_background};
        break;
    case Role::link:
        style.foreground = theme.link_foreground;
        style.attributes |= text_underline;
        break;
    case Role::heading:
    case Role::table_header:
        style.foreground = theme.heading_foreground;
        style.attributes |= text_bold;
        break;
    case Role::marker:
    case Role::rule:
        style.foreground = theme.heading_foreground;
        break;
    }
    return style;
}

void MarkdownView::paint(Painter& painter, const Theme& theme) const
{
    const auto background{resolve(m_background, theme, theme.editor_background)};
    painter.fill_rect(bounds(), background);
    for(auto row{0}; row < bounds().height && m_scroll + row < static_cast<int>(m_lines.size()); ++ row)
    {
        const auto& line{m_lines[static_cast<std::size_t>(m_scroll + row)]};
        const auto y{bounds().y + row};
        if(!line.empty() && line.front().role == Role::code_block)
        {
            painter.fill_rect({bounds().x + 1, y, static_cast<double>(text_width_available()), 1.0},
                              theme.code_background);
        }
        // Prose flows as interface text; tables, rules and code keep to the grid, so their columns line up.
        const auto on_grid{std::any_of(line.begin(), line.end(), [](const Run& run) { return on_the_grid(run); })};
        auto x{bounds().x + 1};
        std::vector<LabelRun> runs;
        for(const auto& run : line)
        {
            // The link Tab went to stands out, as what Enter follows.
            const auto style{m_chosen_link && run.link == *m_chosen_link
                                 ? TextStyle{theme.selection_foreground, theme.selection_background}
                                 : run_style(run.role, theme, background)};
            // Indentation keeps its cells, so nested lists line up whatever the font.
            if(on_grid || (runs.empty() && run.text.find_first_not_of(' ') == std::string::npos))
            {
                painter.draw_text({x, y}, run.text, style);
                x += text_width(run.text);
                continue;
            }
            runs.push_back({run.text, style});
        }
        if(!runs.empty())
        {
            painter.draw_label_runs({x, y}, runs, background);
        }
    }
    // The scrollbar, when the text is longer than the view.
    if(static_cast<int>(m_lines.size()) > bounds().height && bounds().height > 0)
    {
        const auto track_x{bounds().x + bounds().width - 1};
        const auto thumb{scrollbar_thumb(whole(bounds().height), whole(bounds().height),
                                         static_cast<int>(m_lines.size()), m_scroll, max_scroll())};
        painter.draw_scrollbar(
            {track_x, bounds().y, 1, bounds().height},
            {whole(bounds().y) + thumb.start, thumb.length, theme.scrollbar_background, theme.scrollbar_foreground},
            theme);
    }
}

std::unique_ptr<MarkdownView> markdown_view(std::string markdown)
{
    auto view{std::make_unique<MarkdownView>()};
    view->set_markdown(std::move(markdown));
    return view;
}

MarkdownView& markdown_view(std::string markdown, Container& parent)
{
    return parent.add(markdown_view(std::move(markdown)));
}

} // namespace olagarro::oui
