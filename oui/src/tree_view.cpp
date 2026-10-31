#include <oui/tree_view.h>
#include <oui/text_layout.h>
#include <algorithm>
#include <cstdlib>

namespace olagarro::oui
{

TreeNode& TreeNode::add(std::string_view child_text)
{
    children.push_back({std::string{child_text}, {}, false});
    return children.back();
}

TreeNode& TreeView::add(std::string_view text)
{
    m_nodes.push_back({std::string{text}, {}, false});
    return m_nodes.back();
}

void TreeView::clear()
{
    m_nodes.clear();
    m_selected = 0;
}
void TreeView::set_background(ColorRef color)
{
    m_background = color;
}
void TreeView::set_empty_message(std::string message)
{
    m_empty_message = std::move(message);
}
void TreeView::set_show_expanders(bool enabled)
{
    m_show_expanders = enabled;
}
void TreeView::set_indentation(int cells)
{
    m_indentation = std::max(0, cells);
}
void TreeView::set_header(std::string header)
{
    m_header = std::move(header);
}
void TreeView::set_top_padding(int rows)
{
    m_top_padding = std::max(0, rows);
}
void TreeView::set_focus_proxy_active(bool active)
{
    m_focus_proxy_active = active;
}

bool TreeView::is_focusable() const
{
    return true;
}

const std::list<TreeNode>& TreeView::nodes() const
{
    return m_nodes;
}

const TreeNode* TreeView::selected() const
{
    const auto visible{visible_nodes_const()};
    return m_selected < visible.size() ? visible[m_selected].node : nullptr;
}

namespace
{

// Opens what is above `target` under `nodes`; whether it is there.
bool open_up_to(std::list<TreeNode>& nodes, const TreeNode& target)
{
    for(auto& node : nodes)
    {
        if(&node == &target)
        {
            return true;
        }
        if(open_up_to(node.children, target))
        {
            node.expanded = true;
            return true;
        }
    }
    return false;
}

} // namespace

void TreeView::select(const TreeNode& node)
{
    if(!open_up_to(m_nodes, node))
    {
        return;
    }
    const auto visible{visible_nodes_const()};
    for(std::size_t index{}; index < visible.size(); ++ index)
    {
        if(visible[index].node == &node)
        {
            m_selected = index;
            ensure_selection_visible(visible);
            return;
        }
    }
}

bool TreeView::claims_key(const Event& event) const
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || key->control || key->alt || m_nodes.empty())
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
        return true;
    default:
        return false;
    }
}

bool TreeView::handle_event(const Event& event)
{
    const auto* const before{selected()};
    const auto handled{handle_event_as_is(event)};
    if(selected() != before)
    {
        m_selection_changed.fire();
    }
    return handled;
}

bool TreeView::handle_event_as_is(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        const auto visible{visible_nodes_const()};
        if(mouse->wheel_delta != 0)
        {
            const auto maximum{maximum_scroll_offset(scrollable_visual_height(visible))};
            constexpr int wheel_scroll_rows{3};
            m_scroll_offset = static_cast<std::size_t>(
                std::clamp(static_cast<int>(m_scroll_offset) - mouse->wheel_delta * wheel_scroll_rows, 0,
                           static_cast<int>(maximum)));
            select_first_visible(visible);
            return true;
        }
        if(mouse->dragging)
        {
            if(m_dragging_scrollbar)
            {
                const auto total_height{scrollable_visual_height(visible)};
                const auto viewport{visible_height()};
                const auto maximum{maximum_scroll_offset(total_height)};
                const int thumb_height{std::max(1, static_cast<int>(viewport) * static_cast<int>(viewport) /
                                                       static_cast<int>(total_height))};
                const int thumb_range{std::max(0, static_cast<int>(viewport) - thumb_height)};
                const int track_top{whole(bounds().y) + m_top_padding};
                const int thumb_top{std::clamp(whole(mouse->y) - m_scroll_grab, track_top, track_top + thumb_range)};
                if(thumb_range > 0 && maximum > 0)
                {
                    m_scroll_offset =
                        static_cast<std::size_t>((thumb_top - track_top) * static_cast<int>(maximum) / thumb_range);
                }
                select_first_visible(visible);
                return true;
            }
            const auto dy{whole(mouse->y) - m_drag_y};
            if(dy != 0)
            {
                const auto maximum{maximum_scroll_offset(scrollable_visual_height(visible))};
                m_scroll_offset = static_cast<std::size_t>(
                    std::clamp(static_cast<int>(m_scroll_offset) + dy, 0, static_cast<int>(maximum)));
                m_drag_y = whole(mouse->y);
                select_first_visible(visible);
            }
            return true;
        }
        if(mouse->released)
        {
            const bool was_dragging_scrollbar{m_dragging_scrollbar};
            m_dragging_scrollbar = false;
            if(was_dragging_scrollbar)
            {
                return true;
            }
            if(std::abs(mouse->y - m_press_y) <= 1)
            {
                if(mouse->y >= bounds().y + m_top_padding && mouse->y < bounds().y + bounds().height)
                {
                    const auto index{entry_at_visual_row(
                        visible,
                        m_scroll_offset + static_cast<std::size_t>(whole(mouse->y - bounds().y) - m_top_padding),
                        content_view_width(visible))};
                    if(index)
                    {
                        m_selected = *index;
                        ensure_selection_visible(visible);
                        m_activated.fire(*index);
                    }
                }
            }
            return true;
        }
        if(mouse->pressed)
        {
            m_press_y = whole(mouse->y);
            m_drag_y = whole(mouse->y);
            m_dragging_scrollbar = false;
            const auto total_height{scrollable_visual_height(visible)};
            const auto viewport{visible_height()};
            if(total_height > viewport && mouse->x == bounds().x + bounds().width - 1 &&
               mouse->y >= bounds().y + m_top_padding && mouse->y < bounds().y + bounds().height)
            {
                const auto maximum{maximum_scroll_offset(total_height)};
                const int thumb_height{std::max(1, static_cast<int>(viewport) * static_cast<int>(viewport) /
                                                       static_cast<int>(total_height))};
                const int thumb_range{std::max(0, static_cast<int>(viewport) - thumb_height)};
                const int track_top{whole(bounds().y) + m_top_padding};
                const int thumb_top{
                    track_top +
                    (maximum == 0 ? 0 : static_cast<int>(m_scroll_offset) * thumb_range / static_cast<int>(maximum))};
                if(mouse->y >= thumb_top && mouse->y < thumb_top + thumb_height)
                {
                    m_dragging_scrollbar = true;
                    m_scroll_grab = whole(mouse->y) - thumb_top;
                }
                else if(thumb_range > 0 && maximum > 0)
                {
                    const int desired_top{
                        std::clamp(whole(mouse->y) - thumb_height / 2, track_top, track_top + thumb_range)};
                    m_scroll_offset =
                        static_cast<std::size_t>((desired_top - track_top) * static_cast<int>(maximum) / thumb_range);
                    select_first_visible(visible);
                }
                return true;
            }
            if(mouse->y >= bounds().y + m_top_padding && mouse->y < bounds().y + bounds().height)
            {
                const auto index{entry_at_visual_row(
                    visible, m_scroll_offset + static_cast<std::size_t>(whole(mouse->y - bounds().y) - m_top_padding),
                    content_view_width(visible))};
                if(index)
                {
                    m_selected = *index;
                    ensure_selection_visible(visible);
                }
            }
            return true;
        }
        return false;
    }

    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || m_nodes.empty())
    {
        return false;
    }
    const auto visible{visible_nodes()};
    if(visible.empty())
    {
        return false;
    }
    if(m_selected >= visible.size())
    {
        m_selected = visible.size() - 1;
    }
    // At either end an arrow stays in the tree, as Turbo Vision's lists do: leaving it is Tab's work.
    if(key->key == Key::down)
    {
        m_selected = std::min(m_selected + 1, visible.size() - 1);
    }
    else if(key->key == Key::up)
    {
        m_selected = m_selected == 0 ? 0 : m_selected - 1;
    }
    else if(key->key == Key::page_down)
    {
        m_selected = std::min(m_selected + visible_height(), visible.size() - 1);
    }
    else if(key->key == Key::page_up)
    {
        m_selected = m_selected > visible_height() ? m_selected - visible_height() : 0;
    }
    else if(key->key == Key::right && !visible.at(m_selected).node->children.empty())
    {
        visible.at(m_selected).node->expanded = true;
    }
    else if(key->key == Key::left && !visible.at(m_selected).node->children.empty())
    {
        visible.at(m_selected).node->expanded = false;
    }
    else if(key->key == Key::enter)
    {
        m_activated.fire(m_selected);
        return true;
    }
    else
    {
        return false;
    }
    ensure_selection_visible(visible_nodes_const());
    return true;
}

Size TreeView::measure(const Constraints& constraints) const
{
    auto width{0};
    const auto nodes{visible_nodes_const()};
    for(const auto& entry : nodes)
    {
        const int marker_width{m_show_expanders ? 2 : 0};
        width = std::max(width, entry.depth * m_indentation + marker_width + static_cast<int>(entry.node->text.size()));
    }
    const auto constrained_width{std::max(1, std::min(width, whole(constraints.max_width)))};
    return constraints.constrain(
        {static_cast<double>(constrained_width),
         static_cast<double>(m_top_padding + static_cast<int>(visual_height(nodes, constrained_width)))});
}

void TreeView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    const auto nodes{visible_nodes_const()};
    ensure_selection_visible(nodes);
    m_scroll_offset = std::min(m_scroll_offset, maximum_scroll_offset(scrollable_visual_height(nodes)));
}

void TreeView::paint(Painter& painter, const Theme& theme) const
{
    const auto background{resolve(m_background, theme, theme.panel_background)};
    painter.fill_rect(bounds(), background);
    if(!m_header.empty() && m_top_padding > 0)
    {
        painter.fill_rect({bounds().x, bounds().y, bounds().width, 1}, theme.table_header_background);
        painter.draw_label({bounds().x, bounds().y}, m_header,
                           {theme.table_header_foreground, theme.table_header_background, text_bold});
    }
    const auto nodes{visible_nodes_const()};
    if(nodes.empty() && !m_empty_message.empty())
    {
        const auto msg_x{bounds().x +
                         std::max(0, (whole(bounds().width) - static_cast<int>(m_empty_message.size())) / 2)};
        const auto msg_y{bounds().y + std::max(0, whole(bounds().height) / 2 - 1)};
        painter.draw_label({msg_x, msg_y}, m_empty_message, {theme.panel_foreground, background}, Align::center);
        return;
    }
    const auto last{m_scroll_offset + visible_height()};
    const auto content_width{content_view_width(nodes)};
    const auto total_height{visual_height(nodes, content_width)};
    const auto has_scroll{total_height > visible_height()};
    std::size_t visual_row{};
    for(std::size_t index{}; index < nodes.size() && visual_row < last; ++ index)
    {
        const auto& entry{nodes.at(index)};
        const std::string_view prefix{m_show_expanders ? (entry.node->children.empty() ? "  "
                                                          : entry.node->expanded       ? "- "
                                                                                       : "+ ")
                                                       : ""};
        const auto indentation{entry.depth * m_indentation};
        const int marker_width{m_show_expanders ? 2 : 0};
        const auto lines{wrap_text(entry.node->text, std::max(1, content_width - indentation - marker_width))};
        const auto active{(is_focused() || m_focus_proxy_active) && index == m_selected};
        for(std::size_t line{}; line < lines.size(); ++ line, ++ visual_row)
        {
            if(visual_row >= last)
            {
                break;
            }
            if(visual_row < m_scroll_offset)
            {
                continue;
            }
            const auto y{bounds().y + m_top_padding + static_cast<int>(visual_row - m_scroll_offset)};
            const TextStyle style{active ? theme.item_selected_foreground : theme.panel_foreground,
                                  active ? theme.item_selected_background : background};
            if(active)
            {
                painter.fill_rect({bounds().x, y, static_cast<double>(content_width), 1.0},
                                  theme.item_selected_background);
            }
            painter.draw_text({bounds().x + indentation, y}, line == 0 ? prefix : "", style);
            painter.draw_label({bounds().x + indentation + marker_width, y}, lines.at(line), style);
        }
    }
    if(has_scroll)
    {
        const auto track_x{bounds().x + bounds().width - 1};
        const auto viewport{visible_height()};
        const auto thumb_height{
            std::max(1, static_cast<int>(viewport) * static_cast<int>(viewport) / static_cast<int>(total_height))};
        const auto thumb_range{std::max(0, static_cast<int>(viewport) - thumb_height)};
        const auto content_range{static_cast<int>(maximum_scroll_offset(total_height))};
        const auto thumb_y{whole(bounds().y) + m_top_padding +
                           (content_range == 0 ? 0 : static_cast<int>(m_scroll_offset) * thumb_range / content_range)};
        painter.draw_scrollbar({track_x, bounds().y + m_top_padding, 1, static_cast<double>(viewport)},
                               {thumb_y, thumb_height, theme.scrollbar_background, theme.scrollbar_foreground}, theme);
    }
}

void TreeView::append_visible(std::vector<Entry>& entries, TreeNode& node, int depth)
{
    entries.push_back({&node, depth});
    if(node.expanded)
    {
        for(auto& child : node.children)
        {
            append_visible(entries, child, depth + 1);
        }
    }
}

std::vector<TreeView::Entry> TreeView::visible_nodes()
{
    std::vector<Entry> entries;
    for(auto& node : m_nodes)
    {
        append_visible(entries, node, 0);
    }
    return entries;
}

void TreeView::append_visible(std::vector<ConstEntry>& entries, const TreeNode& node, int depth) const
{
    entries.push_back({&node, depth});
    if(node.expanded)
    {
        for(const auto& child : node.children)
        {
            append_visible(entries, child, depth + 1);
        }
    }
}

std::vector<TreeView::ConstEntry> TreeView::visible_nodes_const() const
{
    std::vector<ConstEntry> entries;
    for(const auto& node : m_nodes)
    {
        append_visible(entries, node, 0);
    }
    return entries;
}

std::size_t TreeView::entry_height(const ConstEntry& entry, int available_width) const
{
    const int marker_width{m_show_expanders ? 2 : 0};
    const int text_width{std::max(1, available_width - entry.depth * m_indentation - marker_width)};
    return wrap_text(entry.node->text, text_width).size();
}

std::size_t TreeView::visual_height(const std::vector<ConstEntry>& entries, int available_width) const
{
    std::size_t result{};
    for(const auto& entry : entries)
    {
        result += entry_height(entry, available_width);
    }
    return result;
}

int TreeView::content_view_width(const std::vector<ConstEntry>& entries) const
{
    const int full_width{std::max(1, whole(bounds().width))};
    if(visual_height(entries, full_width) <= visible_height())
    {
        return full_width;
    }
    return std::max(1, full_width - 1);
}

std::size_t TreeView::scrollable_visual_height(const std::vector<ConstEntry>& entries) const
{
    return visual_height(entries, content_view_width(entries));
}

std::optional<std::size_t> TreeView::entry_at_visual_row(const std::vector<ConstEntry>& entries, std::size_t row,
                                                         int available_width) const
{
    std::size_t current{};
    for(std::size_t index{}; index < entries.size(); ++ index)
    {
        const auto height{entry_height(entries.at(index), available_width)};
        if(row < current + height)
        {
            return index;
        }
        current += height;
    }
    return std::nullopt;
}

std::size_t TreeView::visual_row_of_entry(const std::vector<ConstEntry>& entries, std::size_t index,
                                          int available_width) const
{
    std::size_t result{};
    for(std::size_t current{}; current < index && current < entries.size(); ++ current)
    {
        result += entry_height(entries.at(current), available_width);
    }
    return result;
}

void TreeView::select_first_visible(const std::vector<ConstEntry>& entries)
{
    if(const auto index{entry_at_visual_row(entries, m_scroll_offset, content_view_width(entries))})
    {
        m_selected = *index;
    }
}

std::size_t TreeView::visible_height() const
{
    return static_cast<std::size_t>(std::max(1, whole(bounds().height) - m_top_padding));
}

std::size_t TreeView::maximum_scroll_offset(std::size_t visual_row_count) const
{
    const auto height{visible_height()};
    return visual_row_count > height ? visual_row_count - height : 0;
}

void TreeView::ensure_selection_visible(const std::vector<ConstEntry>& entries)
{
    if(entries.empty())
    {
        m_scroll_offset = 0;
        return;
    }
    m_selected = std::min(m_selected, entries.size() - 1);
    const auto width{content_view_width(entries)};
    const auto selected_row{visual_row_of_entry(entries, m_selected, width)};
    const auto selected_height{entry_height(entries.at(m_selected), width)};
    if(selected_row < m_scroll_offset)
    {
        m_scroll_offset = selected_row;
    }
    else if(selected_row + selected_height > m_scroll_offset + visible_height())
    {
        m_scroll_offset = selected_row + selected_height - visible_height();
    }
    m_scroll_offset = std::min(m_scroll_offset, maximum_scroll_offset(scrollable_visual_height(entries)));
}

std::unique_ptr<TreeView> tree_view()
{
    return std::make_unique<TreeView>();
}
TreeView& tree_view(Container& parent)
{
    return parent.add(tree_view());
}

} // namespace olagarro::oui
