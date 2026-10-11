#include <oui/dynamic_grid.h>

#include <algorithm>

namespace olagarro::oui
{

void DynamicGrid::set_column_width(Length width)
{
    m_column_width = width;
}
void DynamicGrid::set_gap(Spacing gap)
{
    m_gap = gap;
}
Size DynamicGrid::cell_size(const Constraints& constraints) const
{
    Size size{};
    for(const auto& child : children())
    {
        const auto value{child->measure(constraints)};
        size.width = std::max(size.width, value.width);
        size.height = std::max(size.height, value.height);
    }
    if(m_column_width.kind() == Length::Kind::cells)
    {
        size.width = std::max(1, static_cast<int>(m_column_width.value()));
    }
    return {std::max(1, size.width), std::max(1, size.height)};
}
int DynamicGrid::column_count(int width, int cell_width) const
{
    return std::max(1, (width + spacing_cells(m_gap)) / (cell_width + spacing_cells(m_gap)));
}
Size DynamicGrid::measure(const Constraints& constraints) const
{
    const auto cell{cell_size(constraints)};
    if(children().empty())
    {
        return {};
    }
    const auto columns{column_count(constraints.max_width, cell.width)};
    const auto rows{static_cast<int>((children().size() + static_cast<std::size_t>(columns) - 1) /
                                     static_cast<std::size_t>(columns))};
    const auto gap{spacing_cells(m_gap)};
    return constraints.constrain({columns * cell.width + (columns - 1) * gap, rows * cell.height + (rows - 1) * gap});
}
void DynamicGrid::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto cell{cell_size({bounds.width, bounds.height})};
    const auto columns{column_count(bounds.width, cell.width)};
    const auto gap{spacing_cells(m_gap)};
    for(std::size_t index{}; index < children().size(); ++ index)
    {
        const auto column{static_cast<int>(index % static_cast<std::size_t>(columns))};
        const auto row{static_cast<int>(index / static_cast<std::size_t>(columns))};
        children().at(index)->arrange(
            {bounds.x + column * (cell.width + gap), bounds.y + row * (cell.height + gap), cell.width, cell.height});
    }
}
std::unique_ptr<DynamicGrid> dynamic_grid()
{
    return std::make_unique<DynamicGrid>();
}
DynamicGrid& dynamic_grid(Container& parent)
{
    return parent.add(dynamic_grid());
}

} // namespace olagarro::oui
