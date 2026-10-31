#include <oui/grid.h>
#include <oui/text_metrics.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace olagarro::oui
{

void Grid::set_columns(std::vector<Length> columns)
{
    m_columns = std::move(columns);
}
void Grid::set_columns(std::initializer_list<Length> columns)
{
    m_columns = columns;
}
void Grid::set_rows(std::vector<Length> rows)
{
    m_rows = std::move(rows);
}
void Grid::set_rows(std::initializer_list<Length> rows)
{
    m_rows = rows;
}
void Grid::set_gap(Spacing gap)
{
    m_gap = gap;
}
namespace
{

// The sizes of the columns, or of the rows, from their lengths: a line without one is as big as its content, `fill`
// and `flex` share what the others leave when `distribute` says so, and are as big as their content when not, as a
// grid measured does. A line not shown takes no room, nor a gap. What a share gives is snapped to `step`; cells and
// pixels are `fixed` of them, `length_across` for columns and `length_down` for rows.
[[nodiscard]] std::vector<double> line_sizes(const std::vector<Length>& lengths, const std::vector<double>& content,
                                             const std::vector<bool>& shown, double available, int gap, bool distribute,
                                             double step, double (*fixed)(Length))
{
    std::vector<double> sizes(content.size(), 0.0);
    auto used{0.0};
    auto lines_shown{0};
    auto factor{0.0f};
    for(std::size_t line{}; line < content.size(); ++ line)
    {
        if(!shown[line])
        {
            continue;
        }
        ++ lines_shown;
        const auto length{line < lengths.size() ? lengths[line] : Length::content()};
        switch(length.kind())
        {
        case Length::Kind::cells:
        case Length::Kind::pixels:
            sizes[line] = fixed(length);
            break;
        case Length::Kind::percent:
            // In doubles: a float loses the pixel a window's step is.
            sizes[line] = std::max(0.0, snap(available * static_cast<double>(length.value()) / 100.0, step));
            break;
        case Length::Kind::fill:
        case Length::Kind::flex:
            if(distribute)
            {
                factor += length.value();
                break;
            }
            sizes[line] = content[line];
            break;
        default:
            sizes[line] = content[line];
            break;
        }
        used += sizes[line];
    }
    used += std::max(0, lines_shown - 1) * gap;
    if(!distribute || factor <= 0.0f)
    {
        return sizes;
    }
    const auto remaining{std::max(0.0, available - used)};
    for(std::size_t line{}; line < content.size(); ++ line)
    {
        const auto length{line < lengths.size() ? lengths[line] : Length::content()};
        if(shown[line] && (length.kind() == Length::Kind::fill || length.kind() == Length::Kind::flex))
        {
            sizes[line] = snap(remaining * static_cast<double>(length.value()) / static_cast<double>(factor), step);
        }
    }
    return sizes;
}

// The room the lines take, with a gap between each two shown.
[[nodiscard]] double total(const std::vector<double>& sizes, const std::vector<bool>& shown, int gap)
{
    auto sum{0.0};
    auto lines_shown{0};
    for(std::size_t line{}; line < sizes.size(); ++ line)
    {
        if(shown[line])
        {
            sum += sizes[line];
            ++ lines_shown;
        }
    }
    return sum + std::max(0, lines_shown - 1) * gap;
}

} // namespace

std::vector<bool> Grid::shown_columns() const
{
    std::vector<bool> shown(m_columns.size(), false);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        shown[child % m_columns.size()] = shown[child % m_columns.size()] || children()[child]->is_visible();
    }
    return shown;
}

std::vector<bool> Grid::shown_rows() const
{
    std::vector<bool> shown((children().size() + m_columns.size() - 1) / m_columns.size(), false);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        shown[child / m_columns.size()] = shown[child / m_columns.size()] || children()[child]->is_visible();
    }
    return shown;
}

std::vector<double> Grid::column_widths(double available, bool distribute_flexible) const
{
    // Each column's content: its widest widget shown.
    std::vector<double> content(m_columns.size(), 0.0);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        if(children()[child]->is_visible())
        {
            auto& widest{content[child % m_columns.size()]};
            widest = std::max(widest, children()[child]->measure({available, available}).width);
        }
    }
    return line_sizes(m_columns, content, shown_columns(), available, spacing_cells(m_gap), distribute_flexible,
                      text_metrics().column_step(), length_across);
}

std::vector<double> Grid::row_heights(const Constraints& constraints, bool distribute_flexible) const
{
    const auto shown{shown_rows()};
    std::vector<double> content(shown.size(), 0.0);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        if(children()[child]->is_visible())
        {
            auto& tallest{content[child / m_columns.size()]};
            tallest = std::max(tallest, children()[child]->measure(constraints).height);
        }
    }
    return line_sizes(m_rows, content, shown, constraints.max_height, spacing_cells(m_gap), distribute_flexible,
                      text_metrics().row_step(), length_down);
}

Size Grid::measure(const Constraints& constraints) const
{
    if(m_columns.empty())
    {
        return {};
    }
    const auto gap{spacing_cells(m_gap)};
    return constraints.constrain({total(column_widths(constraints.max_width, false), shown_columns(), gap),
                                  // Rows that fill take the height there is, measured too: a grid of a list
                                  // between a header and a footer is as tall as it may be.
                                  total(row_heights(constraints, true), shown_rows(), gap)});
}

void Grid::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(m_columns.empty())
    {
        return;
    }
    const auto widths{column_widths(bounds.width)};
    const auto heights{row_heights({bounds.width, bounds.height}, true)};
    const auto columns_shown{shown_columns()};
    const auto rows_shown{shown_rows()};
    const auto gap{spacing_cells(m_gap)};
    auto y{bounds.y};
    for(std::size_t row{}; row < heights.size(); ++ row)
    {
        auto x{bounds.x};
        for(std::size_t column{}; column < widths.size(); ++ column)
        {
            const auto index{row * widths.size() + column};
            if(index == children().size())
            {
                return;
            }
            children()[index]->arrange({x, y, widths[column], heights[row]});
            if(columns_shown[column])
            {
                x += widths[column] + gap;
            }
        }
        if(rows_shown[row])
        {
            y += heights[row] + gap;
        }
    }
}
Rect Grid::paint_bounds() const
{
    Rect result{bounds()};
    for(const auto& child : children())
    {
        if(!child->is_visible())
        {
            continue;
        }
        const auto child_bounds{child->paint_bounds()};
        const double right{std::max(result.x + result.width, child_bounds.x + child_bounds.width)};
        const double bottom{std::max(result.y + result.height, child_bounds.y + child_bounds.height)};
        result.x = std::min(result.x, child_bounds.x);
        result.y = std::min(result.y, child_bounds.y);
        result.width = right - result.x;
        result.height = bottom - result.y;
    }
    return result;
}
std::unique_ptr<Grid> grid()
{
    return std::make_unique<Grid>();
}
Grid& grid(Container& parent)
{
    return parent.add(grid());
}

} // namespace olagarro::oui
