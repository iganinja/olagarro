#include <oui/grid.h>

#include <algorithm>
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
// grid measured does. A line not shown takes no room, nor a gap.
[[nodiscard]] std::vector<int> line_sizes(const std::vector<Length>& lengths, const std::vector<int>& content,
                                          const std::vector<bool>& shown, int available, int gap, bool distribute)
{
    std::vector<int> sizes(content.size(), 0);
    auto used{0};
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
            sizes[line] = std::max(0, static_cast<int>(length.value()));
            break;
        case Length::Kind::percent:
            sizes[line] = std::max(0, static_cast<int>(available * length.value() / 100.0f));
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
    const auto remaining{std::max(0, available - used)};
    for(std::size_t line{}; line < content.size(); ++ line)
    {
        const auto length{line < lengths.size() ? lengths[line] : Length::content()};
        if(shown[line] && (length.kind() == Length::Kind::fill || length.kind() == Length::Kind::flex))
        {
            sizes[line] = static_cast<int>(remaining * length.value() / factor);
        }
    }
    return sizes;
}

// The room the lines take, with a gap between each two shown.
[[nodiscard]] int total(const std::vector<int>& sizes, const std::vector<bool>& shown, int gap)
{
    auto sum{0};
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

std::vector<int> Grid::column_widths(int available, bool distribute_flexible) const
{
    // Each column's content: its widest widget shown.
    std::vector<int> content(m_columns.size(), 0);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        if(children()[child]->is_visible())
        {
            auto& widest{content[child % m_columns.size()]};
            widest = std::max(widest, children()[child]->measure({available, available}).width);
        }
    }
    return line_sizes(m_columns, content, shown_columns(), available, spacing_cells(m_gap), distribute_flexible);
}

std::vector<int> Grid::row_heights(const Constraints& constraints, bool distribute_flexible) const
{
    const auto shown{shown_rows()};
    std::vector<int> content(shown.size(), 0);
    for(std::size_t child{}; child < children().size(); ++ child)
    {
        if(children()[child]->is_visible())
        {
            auto& tallest{content[child / m_columns.size()]};
            tallest = std::max(tallest, children()[child]->measure(constraints).height);
        }
    }
    return line_sizes(m_rows, content, shown, constraints.max_height, spacing_cells(m_gap), distribute_flexible);
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
        const int right{std::max(result.x + result.width, child_bounds.x + child_bounds.width)};
        const int bottom{std::max(result.y + result.height, child_bounds.y + child_bounds.height)};
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
