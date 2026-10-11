#include <oui/diff_view.h>

#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/text_metrics.h>
#include <oui/theme.h>
#include <oui/translation.h>
#include <oui/utf8.h>

#include "unicode_composition.h"

#include <algorithm>
#include <cstdlib>

namespace olagarro::oui
{

namespace
{

// The rows a wheel's notch scrolls, and the columns Left and Right do.
constexpr std::size_t wheel_rows{3};
constexpr int key_columns{4};

// The text cut from its start to fit the cells, with "…" where it was cut, so a path keeps its file's name.
[[nodiscard]] std::string fit_from_start(std::string_view text, int cells)
{
    const auto& metrics{text_metrics()};
    const auto room{cells * metrics.cell_width()};
    if(metrics.width(text) <= room)
    {
        return std::string{text};
    }
    std::size_t start{};
    while(start < text.size())
    {
        static_cast<void>(decode_utf8(text, start));
        auto cut{"…" + std::string{text.substr(start)}};
        if(metrics.width(cut) <= room)
        {
            return cut;
        }
    }
    return {};
}

} // namespace

DiffView::DiffView() = default;

void DiffView::set_texts(std::string left, std::string right)
{
    m_left_text = std::move(left);
    m_right_text = std::move(right);
    m_left_lines = split_lines(m_left_text);
    m_right_lines = split_lines(m_right_text);
    m_rows = diff_rows(m_left_lines, m_right_lines);
    m_differences.clear();
    for(std::size_t row{}; row < m_rows.size(); ++ row)
    {
        if(m_rows[row].kind != DiffKind::same && (row == 0 || m_rows[row - 1].kind == DiffKind::same))
        {
            m_differences.push_back(row);
        }
    }
    m_longest_line = 0;
    for(const auto* lines : {&m_left_lines, &m_right_lines})
    {
        for(const auto line : *lines)
        {
            m_longest_line = std::max(m_longest_line, line_columns(line));
        }
    }
    m_current_row = m_rows.empty() ? 0 : std::min(m_current_row, m_rows.size() - 1);
    m_scroll_row = std::min(m_scroll_row, max_scroll_row());
    m_scroll_column = std::min(m_scroll_column, max_scroll_column());
}

void DiffView::set_titles(std::string left, std::string right)
{
    m_left_title = std::move(left);
    m_right_title = std::move(right);
}

void DiffView::set_tab_width(int columns)
{
    m_tab_width = std::max(1, columns);
}

const std::vector<DiffRow>& DiffView::rows() const
{
    return m_rows;
}

std::size_t DiffView::difference_count() const
{
    return m_differences.size();
}

std::size_t DiffView::current_row() const
{
    return m_current_row;
}

DiffSide DiffView::current_side() const
{
    return m_current_side;
}

void DiffView::set_current_row(std::size_t row)
{
    move_to(row);
}

void DiffView::next_difference()
{
    const auto next{std::upper_bound(m_differences.begin(), m_differences.end(), m_current_row)};
    if(next != m_differences.end())
    {
        show_difference(*next);
    }
}

void DiffView::previous_difference()
{
    // The start of the difference the row is in counts as the one before only from a row after it.
    const auto previous{std::lower_bound(m_differences.begin(), m_differences.end(), m_current_row)};
    if(previous != m_differences.begin())
    {
        show_difference(*std::prev(previous));
    }
}

void DiffView::move_to(std::size_t row)
{
    if(m_rows.empty())
    {
        return;
    }
    m_current_row = std::min(row, m_rows.size() - 1);
    const auto visible{static_cast<std::size_t>(std::max(1, content_rows()))};
    if(m_current_row < m_scroll_row)
    {
        m_scroll_row = m_current_row;
    }
    else if(m_current_row >= m_scroll_row + visible)
    {
        m_scroll_row = m_current_row - visible + 1;
    }
    m_scroll_row = std::min(m_scroll_row, max_scroll_row());
}

void DiffView::show_difference(std::size_t row)
{
    m_current_row = row;
    // Near the top, after a little of what is before it, so as much of it as fits shows below.
    const auto context{std::min<std::size_t>(2, static_cast<std::size_t>(std::max(0, content_rows())) / 3)};
    m_scroll_row = std::min(row >= context ? row - context : 0, max_scroll_row());
}

std::optional<std::size_t> DiffView::difference_at(std::size_t row) const
{
    if(row >= m_rows.size() || m_rows[row].kind == DiffKind::same)
    {
        return std::nullopt;
    }
    const auto start{std::upper_bound(m_differences.begin(), m_differences.end(), row)};
    return static_cast<std::size_t>(std::distance(m_differences.begin(), start)) - 1;
}

int DiffView::content_rows() const
{
    return std::max(0, bounds().height - 2);
}

int DiffView::left_width() const
{
    // The scroll bar takes the last column and the divider the one between the sides.
    return std::max(0, (bounds().width - 2) / 2);
}

int DiffView::right_width() const
{
    return std::max(0, bounds().width - 2 - left_width());
}

int DiffView::gutter_width() const
{
    const auto lines{std::max(m_left_lines.size(), m_right_lines.size())};
    return static_cast<int>(std::to_string(std::max<std::size_t>(lines, 1)).size()) + 1;
}

int DiffView::text_width() const
{
    return std::max(0, std::min(left_width(), right_width()) - gutter_width());
}

std::size_t DiffView::max_scroll_row() const
{
    const auto visible{static_cast<std::size_t>(std::max(0, content_rows()))};
    return m_rows.size() > visible ? m_rows.size() - visible : 0;
}

int DiffView::max_scroll_column() const
{
    return std::max(0, m_longest_line - text_width() + 1);
}

int DiffView::line_columns(std::string_view line) const
{
    int columns{};
    std::size_t index{};
    while(index < line.size())
    {
        const auto codepoint{decode_utf8(line, index)};
        columns += codepoint == U'\t' ? m_tab_width - columns % m_tab_width : terminal_cell_width(codepoint);
    }
    return columns;
}

Size DiffView::measure(const Constraints& constraints) const
{
    return constraints.constrain({std::max(1, constraints.max_width), std::max(1, constraints.max_height)});
}

bool DiffView::is_focusable() const
{
    return true;
}

void DiffView::paint_side(Painter& painter, const Theme& theme, const DiffRow& row, DiffSide side, int x, int y,
                          int width, bool current) const
{
    const auto left{side == DiffSide::left};
    const auto line_number{left ? row.left_line : row.right_line};
    if(!line_number)
    {
        painter.fill_rect({x, y, width, 1}, theme.diff_filler_background);
        return;
    }
    const auto line{(left ? m_left_lines : m_right_lines)[*line_number]};
    const auto& changes{left ? row.left_changes : row.right_changes};
    const TextStyle same{theme.editor_foreground, theme.editor_background};
    const TextStyle removed{theme.diff_removed_foreground, theme.diff_removed_background};
    const TextStyle added{theme.diff_added_foreground, theme.diff_added_background};
    const TextStyle changed{theme.diff_changed_foreground, theme.diff_changed_background};
    // A changed line marks what changed within it as removed or added; one with too little in common to say is
    // removed or added whole.
    const auto marked{left ? removed : added};
    const auto related{!row.left_changes.empty() || !row.right_changes.empty()};
    const auto base{row.kind == DiffKind::same      ? same
                    : row.kind == DiffKind::removed ? removed
                    : row.kind == DiffKind::added   ? added
                    : related                       ? changed
                                                    : marked};

    const auto gutter{std::min(gutter_width(), width)};
    const TextStyle number_style{current ? theme.selection_foreground : theme.line_number_foreground,
                                 current ? theme.selection_background : theme.editor_background};
    painter.fill_rect({x, y, gutter, 1}, number_style.background);
    const auto number{std::to_string(*line_number + 1)};
    painter.draw_text({x + gutter - 1 - static_cast<int>(number.size()), y}, number, number_style);

    const auto text_x{x + gutter};
    const auto text_columns{width - gutter};
    painter.fill_rect({text_x, y, text_columns, 1}, base.background);
    auto range{changes.begin()};
    int column{};
    std::size_t index{};
    while(index < line.size() && column < m_scroll_column + text_columns)
    {
        const auto start{index};
        auto codepoint{decode_utf8(line, index)};
        const auto cells{codepoint == U'\t' ? m_tab_width - column % m_tab_width : terminal_cell_width(codepoint)};
        // Accents stored after their letter are drawn in its cell, as the editor draws them.
        while(cells > 0 && index < line.size())
        {
            auto following{index};
            const auto mark{decode_utf8(line, following)};
            const auto composed{is_diacritic(mark) ? compose(codepoint, mark) : std::nullopt};
            if(!composed)
            {
                break;
            }
            codepoint = *composed;
            index = following;
        }
        while(range != changes.end() && range->second <= start)
        {
            ++ range;
        }
        const auto style{range != changes.end() && range->first <= start ? marked : base};
        // Only what fits whole in the columns shown.
        if(cells > 0 && column >= m_scroll_column && column + cells <= m_scroll_column + text_columns)
        {
            const auto cell_x{text_x + column - m_scroll_column};
            if(codepoint == U'\t')
            {
                painter.fill_rect({cell_x, y, cells, 1}, style.background);
            }
            else
            {
                painter.draw_text({cell_x, y}, encode_utf8(codepoint), style);
            }
        }
        column += cells;
    }
}

void DiffView::paint(Painter& painter, const Theme& theme) const
{
    const auto& area{bounds()};
    if(area.width <= 0 || area.height <= 0)
    {
        return;
    }
    painter.fill_rect(area, theme.editor_background);
    const auto divider_x{area.x + left_width()};
    const auto right_x{divider_x + 1};

    // The bar over the sides, with their names.
    const TextStyle header{theme.table_header_foreground, theme.table_header_background};
    painter.fill_rect({area.x, area.y, area.width, 1}, header.background);
    painter.draw_label({area.x + 1, area.y}, fit_from_start(m_left_title, std::max(0, left_width() - 2)), header);
    painter.draw_label({right_x + 1, area.y}, fit_from_start(m_right_title, std::max(0, right_width() - 2)), header);

    const TextStyle divider{theme.line_number_foreground, theme.editor_background};
    for(int screen_row{}; screen_row < content_rows(); ++ screen_row)
    {
        const auto y{area.y + 1 + screen_row};
        painter.draw_text({divider_x, y}, "│", divider);
        const auto index{m_scroll_row + static_cast<std::size_t>(screen_row)};
        if(index >= m_rows.size())
        {
            continue;
        }
        const auto current{index == m_current_row};
        paint_side(painter, theme, m_rows[index], DiffSide::left, area.x, y, left_width(), current);
        paint_side(painter, theme, m_rows[index], DiffSide::right, right_x, y, right_width(), current);
    }

    if(m_rows.size() > static_cast<std::size_t>(content_rows()) && content_rows() > 0)
    {
        const auto track_height{content_rows()};
        const auto thumb{scrollbar_thumb(track_height, track_height, static_cast<int>(m_rows.size()),
                                         static_cast<int>(m_scroll_row), static_cast<int>(max_scroll_row()))};
        painter.draw_scrollbar(
            {area.x + area.width - 1, area.y + 1, 1, track_height},
            {area.y + 1 + thumb.start, thumb.length, theme.scrollbar_background, theme.scrollbar_foreground}, theme);
    }

    // The bar under them: how many differences, which one the row is in, and the keys that go from one to the next.
    std::string status;
    if(m_differences.empty())
    {
        status = translate("DiffView", "The two texts are the same.");
    }
    else
    {
        const auto count{std::to_string(m_differences.size())};
        if(const auto at{difference_at(m_current_row)})
        {
            status = format_text(translate("DiffView", "Difference {0} of {1}"), {std::to_string(*at + 1), count});
        }
        else
        {
            status = m_differences.size() == 1 ? translate("DiffView", "1 difference")
                                               : format_text(translate("DiffView", "{0} differences"), {count});
        }
        status += "   " + translate("DiffView", "F7 next, Shift+F7 previous");
    }
    const TextStyle footer{theme.status_foreground, theme.status_background};
    const auto footer_y{area.y + area.height - 1};
    if(area.height > 1)
    {
        painter.fill_rect({area.x, footer_y, area.width, 1}, footer.background);
        painter.draw_label({area.x + 1, footer_y}, fit_text(status, std::max(0, area.width - 2)), footer);
    }
}

bool DiffView::claims_key(const Event& event) const
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    return key != nullptr && !key->control && !key->alt && (key->key == Key::f7 || key->key == Key::enter);
}

bool DiffView::handle_event(const Event& event)
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(key->alt)
        {
            return false;
        }
        const auto page{static_cast<std::size_t>(std::max(1, content_rows() - 1))};
        switch(key->key)
        {
        case Key::up:
            move_to(m_current_row > 0 ? m_current_row - 1 : 0);
            return true;
        case Key::down:
            move_to(m_current_row + 1);
            return true;
        case Key::page_up:
            move_to(m_current_row > page ? m_current_row - page : 0);
            return true;
        case Key::page_down:
            move_to(m_current_row + page);
            return true;
        case Key::home:
            move_to(0);
            return true;
        case Key::end:
            move_to(m_rows.empty() ? 0 : m_rows.size() - 1);
            return true;
        case Key::left:
            m_scroll_column = std::max(0, m_scroll_column - key_columns);
            return true;
        case Key::right:
            m_scroll_column = std::min(max_scroll_column(), m_scroll_column + key_columns);
            return true;
        case Key::f7:
            if(key->control)
            {
                return false;
            }
            key->shift ? previous_difference() : next_difference();
            return true;
        case Key::enter:
            if(m_current_row < m_rows.size())
            {
                const auto& row{m_rows[m_current_row]};
                m_activated.fire(m_current_side, m_current_side == DiffSide::left ? row.left_line : row.right_line);
            }
            return true;
        default:
            return false;
        }
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(m_rows.size() > static_cast<std::size_t>(content_rows()) && content_rows() > 0)
        {
            if(const auto to{
                   m_scrollbar.handle(*mouse, {bounds().x + bounds().width - 1, bounds().y + 1, 1, content_rows()},
                                      content_rows(), static_cast<int>(m_rows.size()), static_cast<int>(m_scroll_row),
                                      static_cast<int>(max_scroll_row()), std::max(1, content_rows() - 1))})
            {
                m_scroll_row = static_cast<std::size_t>(*to);
                return true;
            }
        }
        if(mouse->wheel_delta != 0)
        {
            const auto rows{wheel_rows * static_cast<std::size_t>(std::abs(mouse->wheel_delta))};
            m_scroll_row = mouse->wheel_delta > 0 ? (m_scroll_row > rows ? m_scroll_row - rows : 0)
                                                  : std::min(max_scroll_row(), m_scroll_row + rows);
            return true;
        }
        const auto row{mouse->y - bounds().y - 1};
        if(!mouse->pressed || mouse->dragging || row < 0 || row >= content_rows() ||
           mouse->x >= bounds().x + bounds().width - 1)
        {
            return mouse->pressed;
        }
        const auto index{m_scroll_row + static_cast<std::size_t>(row)};
        if(index < m_rows.size())
        {
            m_current_side = mouse->x < bounds().x + left_width() ? DiffSide::left : DiffSide::right;
            move_to(index);
            if(mouse->clicks == 2)
            {
                const auto& chosen{m_rows[index]};
                m_activated.fire(m_current_side,
                                 m_current_side == DiffSide::left ? chosen.left_line : chosen.right_line);
            }
        }
        return true;
    }
    return false;
}

std::unique_ptr<DiffView> diff_view()
{
    return std::make_unique<DiffView>();
}

} // namespace olagarro::oui
