#include <oui/table.h>
#include <oui/text_metrics.h>

#include <oui/label.h>
#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace olagarro::oui
{

namespace
{

} // namespace

Table::Table() = default;

// === Columns ===

std::size_t Table::column_count() const
{
    if(!m_column_widths.empty())
    {
        return m_column_widths.size();
    }
    if(m_headers_visible && !m_headers.empty())
    {
        return m_headers.size();
    }
    return 1;
}

void Table::set_column_width(std::size_t col, int width_cells)
{
    if(col >= m_column_widths.size())
    {
        m_column_widths.resize(col + 1, 4);
    }
    m_column_widths[col] = width_cells;
}

int Table::column_width(std::size_t col) const
{
    return col < m_column_widths.size() ? m_column_widths[col] : 4;
}

void Table::set_stretch_column(std::optional<std::size_t> col)
{
    m_stretch_column = col;
}

void Table::set_gap(Spacing gap)
{
    m_gap = gap;
}

int Table::column_pixel_width(std::size_t col) const
{
    if(m_stretch_column != col)
    {
        return std::max(column_width(col), 1);
    }
    const auto columns{column_count()};
    auto others{0};
    for(std::size_t other{}; other < columns; ++ other)
    {
        if(other != col)
        {
            others += std::max(column_width(other), 1);
        }
    }
    const auto gaps{spacing_cells(m_gap) * static_cast<int>(columns - 1)};
    return std::max(1, whole(bounds().width) - others - gaps);
}

double Table::header_pixel_x(std::size_t col) const
{
    auto x{bounds().x - m_scroll_offset_x};
    for(std::size_t index{}; index < col; ++ index)
    {
        x += column_pixel_width(index) + spacing_cells(m_gap);
    }
    return x;
}

int Table::header_height() const
{
    return m_headers_visible && !m_headers.empty() ? 1 : 0;
}

// === Cells ===

void Table::add_line(std::initializer_list<std::string_view> cells)
{
    for(const auto cell : cells)
    {
        add(label(cell));
    }
}

void Table::set_cell_widget(std::size_t row, std::size_t col, std::unique_ptr<Widget> widget)
{
    const auto index{row * column_count() + col};
    while(child_count() < index + 1)
    {
        add(label(""));
    }
    replace_child(index, std::move(widget));
}

Widget* Table::cell_widget(std::size_t row, std::size_t col)
{
    const auto index{row * column_count() + col};
    return index < child_count() ? children().at(index).get() : nullptr;
}

void Table::clear()
{
    children_mutable().clear();
    clear_all_metadata();
    m_selected_row.reset();
    m_scroll_offset = 0;
    m_scroll_offset_x = 0;
    m_row_tops.clear();
    m_row_heights.clear();
    m_reveal_row.reset();
}

// === Rows and selection ===

std::size_t Table::row_count() const
{
    const auto columns{column_count()};
    return (child_count() + columns - 1) / columns;
}

std::optional<std::size_t> Table::selected_row() const
{
    return m_selected_row;
}

void Table::select_row(std::size_t row)
{
    if(row >= row_count())
    {
        return;
    }
    if(m_selected_row != row)
    {
        m_selected_row = row;
        m_selection_changed.fire(row);
    }
    ensure_row_visible(row);
}

void Table::set_type_ahead(std::function<std::string(std::size_t row)> text_of)
{
    m_type_ahead_text = std::move(text_of);
}

bool Table::highlights_row(std::size_t row) const
{
    return m_selected_row ? *m_selected_row == row : is_focused() && row == 0;
}

bool Table::is_selected_child(const Widget& child) const
{
    for(std::size_t index{}; index < child_count(); ++ index)
    {
        if(children().at(index).get() == &child)
        {
            return highlights_row(index / column_count());
        }
    }
    return false;
}

// === Scrolling ===

std::optional<std::size_t> Table::row_at(double y) const
{
    if(y < bounds().y + header_height())
    {
        return std::nullopt;
    }
    const auto content_y{whole(y - bounds().y) - header_height() + m_scroll_offset};
    for(std::size_t row{}; row < m_row_tops.size(); ++ row)
    {
        if(content_y >= m_row_tops[row] && content_y < m_row_tops[row] + m_row_heights[row])
        {
            return row;
        }
    }
    return std::nullopt;
}
int Table::scroll_offset() const
{
    return m_scroll_offset;
}

void Table::scroll_to(int offset)
{
    // Clamped once the rows are laid out, when arrange knows how far they go.
    m_scroll_offset = std::max(0, offset);
}

void Table::ensure_row_visible(std::size_t row)
{
    if(row >= m_row_tops.size())
    {
        m_reveal_row = row;
        return;
    }
    const auto top{m_row_tops.at(row)};
    const auto bottom{top + m_row_heights.at(row)};
    if(top < m_scroll_offset)
    {
        m_scroll_offset = top;
    }
    else if(bottom > m_scroll_offset + m_viewport_height)
    {
        m_scroll_offset = bottom - m_viewport_height;
    }
}

int Table::max_scroll_offset() const
{
    return std::max(0, m_total_height - m_viewport_height);
}

int Table::scroll_offset_x() const
{
    return m_scroll_offset_x;
}

void Table::scroll_to_x(int offset)
{
    m_scroll_offset_x = std::clamp(offset, 0, max_scroll_offset_x());
}

int Table::max_scroll_offset_x() const
{
    return std::max(0, m_total_width - m_viewport_width);
}

void Table::set_horizontal_scroll_enabled(bool enabled)
{
    m_horizontal_scroll_enabled = enabled;
}

bool Table::horizontal_scroll_enabled() const
{
    return m_horizontal_scroll_enabled;
}

// === Headers ===

void Table::set_headers(std::vector<std::string> headers)
{
    m_headers = std::move(headers);
    m_headers_visible = true;
}

void Table::set_headers_visible(bool visible)
{
    m_headers_visible = visible;
}

bool Table::headers_visible() const
{
    return m_headers_visible;
}

void Table::set_headers_clickable(bool clickable)
{
    m_headers_clickable = clickable;
}

bool Table::headers_clickable() const
{
    return m_headers_clickable;
}

// === Sorting ===

void Table::set_column_sort(std::size_t col, SortOrder order, CompareFunction custom_compare)
{
    if(col >= m_sort_specs.size())
    {
        m_sort_specs.resize(col + 1);
    }
    m_sort_specs[col].enabled = true;
    m_sort_specs[col].order = order;
    m_sort_specs[col].compare = std::move(custom_compare);
}

SortOrder Table::column_sort_order(std::size_t col) const
{
    if(col >= m_sort_specs.size() || !m_sort_specs[col].enabled)
    {
        return SortOrder::none;
    }
    return m_sort_specs[col].order;
}

std::size_t Table::sort_column_count() const
{
    return static_cast<std::size_t>(
        std::count_if(m_sort_specs.begin(), m_sort_specs.end(), [](const ColumnSort& spec) { return spec.enabled; }));
}

void Table::apply_sort(std::size_t col)
{
    if(col >= m_sort_specs.size() || !m_sort_specs[col].enabled || row_count() < 2)
    {
        return;
    }
    const auto columns{column_count()};
    const auto rows{row_count()};
    const auto& sort{m_sort_specs[col]};
    std::vector<std::size_t> order(rows);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [this, columns, col, &sort](std::size_t left, std::size_t right)
                     {
                         const auto left_index{left * columns + col};
                         const auto right_index{right * columns + col};
                         if(left_index >= child_count() || right_index >= child_count())
                         {
                             return false;
                         }
                         const auto* const lhs{children().at(left_index).get()};
                         const auto* const rhs{children().at(right_index).get()};
                         int comparison{};
                         if(sort.compare)
                         {
                             comparison = sort.compare(lhs, rhs);
                         }
                         else if(const auto* const lhs_label{dynamic_cast<const Label*>(lhs)})
                         {
                             if(const auto* const rhs_label{dynamic_cast<const Label*>(rhs)})
                             {
                                 comparison = lhs_label->text().compare(rhs_label->text());
                             }
                         }
                         return sort.order == SortOrder::ascending ? comparison < 0 : comparison > 0;
                     });

    std::vector<std::unique_ptr<Widget>> sorted_children;
    sorted_children.reserve(child_count());
    decltype(m_metadata) sorted_metadata;
    std::optional<std::size_t> sorted_selection;
    for(std::size_t destination_row{}; destination_row < order.size(); ++ destination_row)
    {
        const auto source_row{order.at(destination_row)};
        for(std::size_t cell{}; cell < columns; ++ cell)
        {
            const auto source_index{source_row * columns + cell};
            if(source_index < child_count())
            {
                sorted_children.push_back(std::move(children_mutable().at(source_index)));
            }
        }
        for(const auto& [position, values] : m_metadata)
        {
            if(position.first == source_row)
            {
                sorted_metadata[{destination_row, position.second}] = values;
            }
        }
        if(m_selected_row == source_row)
        {
            sorted_selection = destination_row;
        }
    }
    children_mutable() = std::move(sorted_children);
    m_metadata = std::move(sorted_metadata);
    m_selected_row = sorted_selection;
}

// === Metadata ===

void Table::set_metadata(std::size_t row, std::size_t col, std::string_view key, MetadataValue value)
{
    m_metadata[{row, col}][std::string{key}] = std::move(value);
}

const Table::MetadataValue* Table::metadata(std::size_t row, std::size_t col, std::string_view key) const
{
    const auto cell{m_metadata.find({row, col})};
    if(cell == m_metadata.end())
    {
        return nullptr;
    }
    const auto value{cell->second.find(std::string{key})};
    return value == cell->second.end() ? nullptr : &value->second;
}

void Table::clear_metadata(std::size_t row, std::size_t col)
{
    m_metadata.erase({row, col});
}

void Table::clear_all_metadata()
{
    m_metadata.clear();
}

Color Table::background_color(const Theme& theme) const
{
    return m_background.resolve(theme);
}

void Table::set_colors(ColorRef background, ColorRef foreground)
{
    m_background = background;
    m_foreground = foreground;
}

// === Events ===

bool Table::is_focusable() const
{
    return true;
}

bool Table::claims_key(const Event& event) const
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || key->control || key->alt || row_count() == 0)
    {
        return false;
    }
    switch(key->key)
    {
    case Key::up:
    case Key::down:
    case Key::left:
    case Key::right:
    case Key::page_up:
    case Key::page_down:
    case Key::home:
    case Key::end:
        return true;
    default:
        return false;
    }
}

bool Table::handle_event(const Event& event)
{
    if(const auto* const text{std::get_if<TextEvent>(&event)};
       text != nullptr && m_type_ahead_text && !text->control && !text->alt && text->codepoint >= U' ')
    {
        const auto count{row_count()};
        // With nothing selected the search starts at the first row.
        const auto current{m_selected_row.value_or(count == 0 ? 0 : count - 1)};
        if(const auto row{m_type_ahead.find(text->codepoint, count, current, m_type_ahead_text)})
        {
            select_row(*row);
        }
        return true;
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(mouse->wheel_delta != 0)
        {
            m_scroll_offset = std::clamp(m_scroll_offset - mouse->wheel_delta, 0, max_scroll_offset());
            return true;
        }
        if(m_total_height > m_viewport_height)
        {
            if(const auto to{m_scrollbar.handle(
                   *mouse, {bounds().x + bounds().width - 1, bounds().y, 1, bounds().height}, whole(bounds().height),
                   m_total_height, m_scroll_offset, max_scroll_offset(), std::max(1, m_viewport_height - 1))})
            {
                m_scroll_offset = *to;
                return true;
            }
        }
        if((m_pressed_header || m_column_drag ||
            (mouse->pressed && whole(mouse->y - bounds().y) == 0 && header_height() == 1)) &&
           !mouse->right)
        {
            if(handle_header_mouse(*mouse))
            {
                return true;
            }
        }
        if(!mouse->pressed)
        {
            return false;
        }
        // The horizontal scrollbar's track.
        if(m_horizontal_scroll_enabled && m_total_width > m_viewport_width && bounds().width > 0 &&
           whole(mouse->y - (bounds().y + bounds().height - 1)) == 0)
        {
            const auto ratio{(mouse->x - bounds().x) / static_cast<double>(bounds().width)};
            scroll_to_x(static_cast<int>(ratio * max_scroll_offset_x()));
            return true;
        }
        if(header_height() == 1 && whole(mouse->y - bounds().y) == 0)
        {
            return false;
        }
        const auto row{row_at(mouse->y)};
        if(!row)
        {
            return false;
        }
        select_row(*row);
        if(mouse->clicks == 2)
        {
            m_row_activated.fire(*row);
        }
        return true;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    const auto rows{row_count()};
    if(key == nullptr || rows == 0)
    {
        return false;
    }
    // With nothing selected the first row stands as the selection, as the table shows it when focused.
    const auto current{m_selected_row.value_or(0)};
    // A page is the rows that fit, taking rows as one line, as rows of text are.
    const auto page{static_cast<std::size_t>(std::max(1, m_viewport_height))};
    switch(key->key)
    {
    // At either end an arrow stays in the list, as Turbo Vision's: leaving it is Tab's work.
    case Key::up:
        select_row(current == 0 ? 0 : current - 1);
        return true;
    case Key::down:
        select_row(current + 1 >= rows ? rows - 1 : current + 1);
        return true;
    case Key::page_up:
        select_row(current > page ? current - page : 0);
        return true;
    case Key::page_down:
        select_row(std::min(current + page, rows - 1));
        return true;
    case Key::home:
        select_row(0);
        return true;
    case Key::end:
        select_row(rows - 1);
        return true;
    case Key::enter:
        if(!m_selected_row)
        {
            return false;
        }
        m_row_activated.fire(*m_selected_row);
        return true;
    case Key::left:
    case Key::right:
        if(!m_horizontal_scroll_enabled)
        {
            return false;
        }
        scroll_to_x(m_scroll_offset_x + (key->key == Key::left ? -1 : 1));
        return true;
    default:
        return false;
    }
}

// === Layout ===

Size Table::measure(const Constraints& constraints) const
{
    const auto columns{column_count()};
    const auto gap{spacing_cells(m_gap)};
    auto width{0.0};
    for(std::size_t col{}; col < columns; ++ col)
    {
        auto column{static_cast<double>(std::max(column_width(col), 1))};
        if(m_stretch_column == col)
        {
            // What the column's cells need, and one more for the scrollbar.
            column = 1.0;
            for(std::size_t index{col}; index < child_count(); index += columns)
            {
                column = std::max(column, child_at(index).measure(Constraints{}).width + 1);
            }
        }
        width += column + (col + 1 < columns ? gap : 0);
    }
    auto height{static_cast<double>(header_height())};
    for(std::size_t row{}; row < row_count(); ++ row)
    {
        auto row_height{0.0};
        for(std::size_t col{}; col < columns; ++ col)
        {
            const auto index{row * columns + col};
            if(index < child_count())
            {
                row_height = std::max(row_height,
                                      child_at(index).measure({constraints.max_width, constraints.max_height}).height);
            }
        }
        height += row_height + (row + 1 < row_count() ? gap : 0);
    }
    return constraints.constrain({width, height});
}

void Table::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto columns{column_count()};
    const auto rows{row_count()};
    const auto gap{spacing_cells(m_gap)};
    std::vector<int> widths(columns);
    m_total_width = 0;
    for(std::size_t col{}; col < columns; ++ col)
    {
        widths[col] = column_pixel_width(col);
        m_total_width += widths[col] + (col + 1 < columns ? gap : 0);
    }
    if(m_total_width > bounds.width)
    {
        m_horizontal_scroll_enabled = true;
    }
    std::vector<int> heights(rows, 0);
    for(std::size_t row{}; row < rows; ++ row)
    {
        for(std::size_t col{}; col < columns; ++ col)
        {
            const auto index{row * columns + col};
            if(index < child_count())
            {
                heights[row] = std::max(
                    heights[row], whole(child_at(index).measure({static_cast<double>(widths[col]), 100.0}).height));
            }
        }
    }
    m_viewport_width = whole(bounds.width);
    // The rows scroll under the header, so the viewport is what is left below it.
    m_viewport_height = std::max(0, whole(bounds.height) - header_height());
    m_row_heights = heights;
    m_row_tops.assign(rows, 0);
    m_total_height = 0;
    for(std::size_t row{}; row < rows; ++ row)
    {
        m_row_tops[row] = m_total_height;
        m_total_height += heights[row] + (row + 1 < rows ? gap : 0);
    }
    m_scroll_offset = std::min(m_scroll_offset, max_scroll_offset());
    m_scroll_offset_x = std::min(m_scroll_offset_x, max_scroll_offset_x());
    if(m_reveal_row && m_viewport_height > 0)
    {
        const auto row{*m_reveal_row};
        m_reveal_row.reset();
        ensure_row_visible(row);
    }
    for(std::size_t row{}; row < rows; ++ row)
    {
        auto x{bounds.x - m_scroll_offset_x};
        const auto y{bounds.y + header_height() + m_row_tops[row] - m_scroll_offset};
        for(std::size_t col{}; col < columns; ++ col)
        {
            const auto index{row * columns + col};
            if(index < child_count())
            {
                child_at(index).arrange({x, y, static_cast<double>(widths[col]), static_cast<double>(heights[row])});
            }
            x += widths[col] + gap;
        }
    }
}

// === Painting ===

void Table::paint(Painter& painter, const Theme& theme) const
{
    painter.fill_rect(bounds(), m_background.resolve(theme));
    const auto columns{column_count()};
    const Rect content{bounds().x, bounds().y + header_height(), bounds().width,
                       static_cast<double>(m_viewport_height)};
    const auto background{background_color(theme)};
    const auto foreground{m_foreground.resolve(theme)};
    painter.push_clip(content);
    for(std::size_t row{}; row < m_row_tops.size(); ++ row)
    {
        const auto y{content.y + m_row_tops[row] - m_scroll_offset};
        if(y + m_row_heights[row] <= content.y || y >= content.y + content.height)
        {
            continue;
        }
        const auto selected{highlights_row(row)};
        if(selected)
        {
            painter.fill_rect({bounds().x, y, bounds().width, static_cast<double>(m_row_heights[row])},
                              theme.item_selected_background);
        }
        for(std::size_t col{}; col < columns; ++ col)
        {
            const auto index{row * columns + col};
            if(index >= child_count())
            {
                break;
            }
            const auto& cell{child_at(index)};
            if(!cell.is_visible())
            {
                continue;
            }
            // Text cells take the table's colours and are cut to their column; other widgets draw themselves,
            // asking is_selected_child whether their row is selected.
            if(const auto* const text{dynamic_cast<const Label*>(&cell)})
            {
                // The label's own colour and weight, as a quieter cell or a heading row; selected, the row's
                // colours win so the selection reads the same across it.
                TextStyle style{selected ? theme.item_selected_foreground : foreground,
                                selected ? theme.item_selected_background : background};
                if(const auto color{text->color()}; color && !selected)
                {
                    style.foreground = color->resolve(theme);
                }
                if(text->is_bold())
                {
                    style.attributes |= text_bold;
                }
                auto x{cell.bounds().x};
                auto width{cell.bounds().width};
                if(const auto marker{text->marker()})
                {
                    painter.draw_marker({x, y}, *marker, style);
                    x += marker->columns;
                    width -= marker->columns;
                }
                // Held to its column, as a proportional font may draw past the cells it counts.
                painter.push_clip({x, y, std::max(0.0, width), static_cast<double>(m_row_heights[row])});
                painter.draw_label(Point{x, y}, fit_text(text->text(), std::max(0, whole(width))), style);
                painter.pop_clip();
            }
            else
            {
                cell.paint(painter, theme);
            }
        }
    }
    painter.pop_clip();
    paint_headers(painter, theme);
    if(m_total_height > m_viewport_height)
    {
        paint_scrollbar(painter, theme);
    }
    if(m_horizontal_scroll_enabled && m_total_width > m_viewport_width)
    {
        paint_horizontal_scrollbar(painter, theme);
    }
}

void Table::set_sorted_by_owner(bool by_owner)
{
    m_sorted_by_owner = by_owner;
}

void Table::set_columns_resizable(bool resizable)
{
    m_columns_resizable = resizable;
}

std::optional<std::size_t> Table::edge_at(double x, int cell_offset) const
{
    // A cell can be wide, so the edge takes only its last pixels and the first ones of the next; in a terminal a cell
    // is the finest place, and both cells round the edge take it.
    const auto cell{std::max(1, text_metrics().cell_width())};
    const auto grab{std::max(1, cell / 3)};
    for(std::size_t col{}; col < column_count(); ++ col)
    {
        // How far the pointer is past the edge, in those units: by its cell and its place in it, as the table may
        // start between cells.
        const auto right{header_pixel_x(col) + column_pixel_width(col)};
        const auto past{static_cast<int>(std::lround((whole(x) - right) * cell)) + cell_offset};
        if(past >= -grab && past < grab)
        {
            return col;
        }
    }
    return std::nullopt;
}

bool Table::handle_header_mouse(const MouseEvent& mouse)
{
    if(mouse.pressed)
    {
        // A press may be the start of a drag on an edge or of a click on a header: which, the pointer says next.
        m_pressed_header.reset();
        m_column_drag.reset();
        if(m_columns_resizable)
        {
            if(const auto edge{edge_at(mouse.x, mouse.cell_offset)})
            {
                // The stretch column takes what is left, so its edge moves the column after it instead.
                const auto stretched{m_stretch_column == edge && *edge + 1 < column_count()};
                const auto col{stretched ? *edge + 1 : *edge};
                if(m_stretch_column != col)
                {
                    m_column_drag = ColumnDrag{col, stretched ? -1 : 1, mouse.x, column_pixel_width(col), false};
                }
            }
        }
        if(m_headers_clickable)
        {
            for(std::size_t col{}; col < m_headers.size(); ++ col)
            {
                const auto left{header_pixel_x(col)};
                if(mouse.x >= left && mouse.x < left + column_pixel_width(col))
                {
                    m_pressed_header = col;
                }
            }
        }
        return m_pressed_header || m_column_drag;
    }
    if(m_column_drag && mouse.dragging && (m_column_drag->moved || whole(mouse.x) != whole(m_column_drag->start_x)))
    {
        // Moved, it is a resize and no longer a click.
        m_column_drag->moved = true;
        m_pressed_header.reset();
        set_column_width(m_column_drag->col,
                         std::max(2, m_column_drag->start_width +
                                         m_column_drag->direction * whole(mouse.x - m_column_drag->start_x)));
        arrange(bounds());
        return true;
    }
    if(!mouse.released)
    {
        return m_pressed_header || m_column_drag;
    }
    // Let go: the end of a resize, or a click on the header it started on.
    if(m_column_drag && m_column_drag->moved)
    {
        m_column_resized.fire(m_column_drag->col, column_width(m_column_drag->col));
    }
    else if(m_pressed_header)
    {
        const auto col{*m_pressed_header};
        const auto left{header_pixel_x(col)};
        if(whole(mouse.y - bounds().y) == 0 && mouse.x >= left && mouse.x < left + column_pixel_width(col))
        {
            click_header(col);
        }
    }
    m_pressed_header.reset();
    m_column_drag.reset();
    return true;
}

void Table::click_header(std::size_t col)
{
    const auto order{column_sort_order(col) == SortOrder::ascending    ? SortOrder::descending
                     : column_sort_order(col) == SortOrder::descending ? SortOrder::none
                                                                       : SortOrder::ascending};
    if(m_sorted_by_owner)
    {
        m_column_clicked.fire(col, order);
        return;
    }
    set_column_sort(col, order);
    if(order != SortOrder::none)
    {
        apply_sort(col);
    }
    m_column_clicked.fire(col, order);
}

void Table::paint_headers(Painter& painter, const Theme& theme) const
{
    if(header_height() == 0)
    {
        return;
    }
    painter.fill_rect({bounds().x, bounds().y, bounds().width, 1}, theme.table_header_background);
    painter.push_clip({bounds().x, bounds().y, bounds().width, 1});
    auto right{bounds().x};
    for(std::size_t col{}; col < m_headers.size(); ++ col)
    {
        const auto order{column_sort_order(col)};
        HeaderLook look;
        look.text = m_headers[col];
        if(order != SortOrder::none)
        {
            look.sort = order == SortOrder::ascending ? Icon::sorted_up : Icon::sorted_down;
        }
        look.pressed = m_pressed_header == std::optional<std::size_t>{col};
        const Rect cell{header_pixel_x(col), bounds().y, static_cast<double>(column_pixel_width(col)), 1.0};
        painter.draw_header(cell, look, theme);
        right = std::max(right, cell.x + cell.width);
    }
    // Past the last column the header row goes on, empty, to the edge.
    if(right < bounds().x + bounds().width)
    {
        painter.draw_header({right, bounds().y, bounds().x + bounds().width - right, 1}, {}, theme);
    }
    painter.pop_clip();
}

void Table::paint_scrollbar(Painter& painter, const Theme& theme) const
{
    if(bounds().width <= 0 || bounds().height <= 0 || m_total_height <= 0)
    {
        return;
    }
    const auto track_x{bounds().x + bounds().width - 1};
    const auto thumb{scrollbar_thumb(whole(bounds().height), whole(bounds().height), m_total_height, m_scroll_offset,
                                     max_scroll_offset())};
    painter.draw_scrollbar(
        {track_x, bounds().y, 1, bounds().height},
        {whole(bounds().y) + thumb.start, thumb.length, theme.scrollbar_background, theme.scrollbar_foreground}, theme);
}

void Table::paint_horizontal_scrollbar(Painter& painter, const Theme& theme) const
{
    if(bounds().width <= 0 || bounds().height <= 0 || m_total_width <= 0)
    {
        return;
    }
    const auto y{bounds().y + bounds().height - 1};
    const auto track_width{whole(bounds().width)};
    const auto thumb_width{std::max(1, track_width * track_width / m_total_width)};
    const auto thumb_range{std::max(0, track_width - thumb_width)};
    const auto content_range{max_scroll_offset_x()};
    const auto thumb_x{whole(bounds().x) + (content_range == 0 ? 0 : m_scroll_offset_x * thumb_range / content_range)};
    painter.draw_scrollbar({bounds().x, y, static_cast<double>(track_width), 1.0},
                           {thumb_x, thumb_width, theme.scrollbar_background, theme.scrollbar_foreground}, theme);
}

std::unique_ptr<Table> table(std::vector<std::string> headers)
{
    auto result{std::make_unique<Table>()};
    if(!headers.empty())
    {
        result->set_headers(std::move(headers));
    }
    return result;
}

Table& table(std::vector<std::string> headers, Container& parent)
{
    return parent.add(table(std::move(headers)));
}

} // namespace olagarro::oui
