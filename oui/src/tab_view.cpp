#include <oui/tab_view.h>
#include <oui/text_metrics.h>
#include <oui/utf8.h>
#include <oui/text_layout.h>
#include <algorithm>
namespace olagarro::oui
{

namespace
{

[[nodiscard]] double title_columns(std::string_view title)
{
    return text_metrics().label_width(title);
}

} // namespace

void TabView::set_close_on_right_click(bool enabled)
{
    m_close_on_right_click = enabled;
}
std::size_t TabView::selected_index() const
{
    return m_selected;
}
std::string_view TabView::title(std::size_t index) const
{
    return m_titles.at(index);
}
void TabView::set_title(std::size_t index, std::string_view title)
{
    m_titles.at(index) = title;
}
void TabView::set_highlight_predicate(std::function<bool(std::size_t)> predicate)
{
    m_is_highlighted = std::move(predicate);
}
void TabView::close_tab(std::size_t index)
{
    static_cast<void>(take_tab(index));
}
std::unique_ptr<Widget> TabView::take_tab(std::size_t index)
{
    if(index >= child_count())
    {
        return nullptr;
    }
    const auto was_selected{index == m_selected};
    m_stepping.clear();
    auto content{remove_at(index)};
    m_titles.erase(m_titles.begin() + static_cast<std::ptrdiff_t>(index));
    m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), content.get()), m_recent.end());
    if(child_count() == 0)
    {
        m_selected = 0;
        return content;
    }
    // The selected tab stays selected; when it is the one that goes, the one used before it takes its place, as
    // editors do.
    if(was_selected)
    {
        m_selected = std::min(index, child_count() - 1);
        if(!m_recent.empty())
        {
            m_selected = index_of(m_recent.front());
        }
    }
    else if(index < m_selected)
    {
        -- m_selected;
    }
    m_selection_changed.fire(m_selected);
    return content;
}
void TabView::select(std::size_t index)
{
    m_stepping.clear();
    if(index < child_count())
    {
        const auto changed{index != m_selected};
        m_selected = index;
        const auto* const content{&child_at(index)};
        m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), content), m_recent.end());
        m_recent.insert(m_recent.begin(), content);
        ensure_selected_visible();
        if(changed)
        {
            m_selection_changed.fire(m_selected);
        }
    }
}
void TabView::ensure_selected_visible()
{
    auto total_width{0.0};
    for(std::size_t index{}; index < m_titles.size(); ++ index)
    {
        total_width += tab_width(index);
    }
    const auto max_scroll{std::max(0.0, total_width - bounds().width)};
    m_header_scroll = std::min(m_header_scroll, max_scroll);
    auto x{bounds().x - m_header_scroll};
    for(std::size_t index{}; index < m_selected; ++ index)
    {
        x += tab_width(index);
    }
    const auto selected_width{tab_width(m_selected)};
    const auto header_end{bounds().x + bounds().width};
    if(x < bounds().x)
    {
        m_header_scroll -= bounds().x - x;
    }
    else if(x + selected_width > header_end)
    {
        m_header_scroll += (x + selected_width) - header_end;
    }
    if(m_header_scroll < 0)
    {
        m_header_scroll = 0;
    }
}
bool TabView::is_focusable() const
{
    return true;
}
bool TabView::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        // A tab pressed by its title can be dragged: off its own title it moves, and letting go says where.
        if(m_pressed_tab && mouse->dragging)
        {
            m_dragging_tab = m_dragging_tab || whole(mouse->y - bounds().y) != 0 || tab_at(mouse->x) != m_pressed_tab;
            if(m_dragging_tab && *m_pressed_tab < m_titles.size())
            {
                // The image is placed by the pointer's cell and how far into it the pointer is.
                set_drag_image(DragImage{m_titles[*m_pressed_tab],
                                         {static_cast<double>(whole(mouse->x)), static_cast<double>(whole(mouse->y))},
                                         mouse->cell_offset});
            }
            return true;
        }
        if(m_pressed_tab && mouse->released)
        {
            const auto index{*m_pressed_tab};
            const auto dragged{m_dragging_tab};
            m_pressed_tab.reset();
            m_dragging_tab = false;
            set_drag_image(std::nullopt);
            if(dragged)
            {
                m_tab_dropped.fire(index, Point{mouse->x, mouse->y});
            }
            return true;
        }
        if(!m_header_visible || !mouse->pressed || whole(mouse->y - bounds().y) != 0 || mouse->x < bounds().x ||
           mouse->x >= bounds().x + bounds().width)
            return false;
        auto x{bounds().x - m_header_scroll};
        for(std::size_t index{}; index < m_titles.size(); ++ index)
        {
            const auto title_len{title_columns(m_titles.at(index))};
            const auto width{tab_width(index)};
            if(mouse->x < x + width)
            {
                const auto close_x{x + title_len + 2};
                if(m_closable && ((mouse->right && m_close_on_right_click) || whole(mouse->x - close_x) == 0))
                {
                    select(index);
                    m_close_requested.fire(index);
                    return true;
                }
                select(index);
                m_tab_activated.fire(index);
                m_pressed_tab = index;
                m_dragging_tab = false;
                return true;
            }
            x += width;
        }
        return false;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || child_count() == 0)
        return false;
    // Left and Right when the content leaves them unused, Ctrl+Page Up and Ctrl+Page Down always, as
    // browsers and editors switch tabs.
    if(key->key == Key::left || (key->key == Key::page_up && key->control))
    {
        select(m_selected == 0 ? child_count() - 1 : m_selected - 1);
        return true;
    }
    if(key->key == Key::right || (key->key == Key::page_down && key->control))
    {
        select((m_selected + 1) % child_count());
        return true;
    }
    return false;
}
Size TabView::measure(const Constraints& constraints) const
{
    return child_count() == 0
               ? Size{}
               : constraints.constrain({child_at(m_selected).measure(constraints).width,
                                        child_at(m_selected).measure(constraints).height + header_rows()});
}
void TabView::arrange(const Rect& bounds)
{
    Widget::arrange(bounds);
    if(child_count() != 0u)
    {
        ensure_selected_visible();
    }
    if(child_count() != 0)
        child_at(m_selected)
            .arrange({bounds.x, bounds.y + header_rows(), bounds.width, std::max(0.0, bounds.height - header_rows())});
}
void TabView::set_closable(bool closable)
{
    m_closable = closable;
}
void TabView::set_header_visible(bool visible)
{
    m_header_visible = visible;
}
bool TabView::header_visible() const
{
    return m_header_visible;
}
double TabView::header_rows() const
{
    return m_header_visible ? 1 : 0;
}
double TabView::tab_width(std::size_t index) const
{
    return title_columns(m_titles.at(index)) + (m_closable ? 4 : 2);
}
void TabView::paint(Painter& painter, const Theme& theme) const
{
    if(m_header_visible)
    {
        painter.push_clip({bounds().x, bounds().y, bounds().width, 1});
        auto x{bounds().x - m_header_scroll};
        for(std::size_t index{}; index < m_titles.size(); ++ index)
        {
            painter.draw_tab(
                {x, bounds().y, tab_width(index), 1},
                {m_titles.at(index), index == m_selected, m_closable, m_is_highlighted && m_is_highlighted(index)},
                theme);
            x += tab_width(index);
        }
        painter.pop_clip();
    }
    if(child_count() != 0)
        child_at(m_selected).paint(painter, theme);
}
void TabView::collect_focusable(std::vector<Widget*>& widgets)
{
    Widget::collect_focusable(widgets);
    if(child_count() != 0)
        child_at(m_selected).collect_focusable(widgets);
}
Widget* TabView::hit_test(double x, double y)
{
    if(Widget::hit_test(x, y) == nullptr)
        return nullptr;
    if(child_count() != 0)
        if(auto* const result{child_at(m_selected).hit_test(x, y)})
            return result;
    return this;
}
void TabView::move_tab(std::size_t from, std::size_t to)
{
    if(from >= child_count() || to >= child_count() || from == to)
    {
        return;
    }
    const auto selected{m_selected};
    auto& children{children_mutable()};
    const auto rotate{
        [from, to](auto& items)
        {
            const auto first{items.begin()};
            if(from < to)
            {
                std::rotate(first + static_cast<std::ptrdiff_t>(from), first + static_cast<std::ptrdiff_t>(from) + 1,
                            first + static_cast<std::ptrdiff_t>(to) + 1);
            }
            else
            {
                std::rotate(first + static_cast<std::ptrdiff_t>(to), first + static_cast<std::ptrdiff_t>(from),
                            first + static_cast<std::ptrdiff_t>(from) + 1);
            }
        }};
    rotate(children);
    rotate(m_titles);
    // The selected tab keeps the selection wherever it now is.
    if(selected == from)
    {
        m_selected = to;
    }
    else if(from < selected && selected <= to)
    {
        -- m_selected;
    }
    else if(to <= selected && selected < from)
    {
        ++ m_selected;
    }
}
std::size_t TabView::index_of(const Widget* content) const
{
    for(std::size_t index{}; index < child_count(); ++ index)
    {
        if(&child_at(index) == content)
        {
            return index;
        }
    }
    return 0;
}
void TabView::step_recent(int step)
{
    if(child_count() < 2)
    {
        return;
    }
    auto stepping{std::move(m_stepping)};
    auto position{m_step};
    if(stepping.empty())
    {
        for(const auto index : recent_order())
        {
            stepping.push_back(&child_at(index));
        }
        position = 0;
    }
    const auto count{static_cast<int>(stepping.size())};
    position = static_cast<std::size_t>(((static_cast<int>(position) + step) % count + count) % count);
    const auto* const chosen{stepping.at(position)};
    select(index_of(chosen));
    m_recent = {chosen};
    for(const auto* const content : stepping)
    {
        if(content != chosen)
        {
            m_recent.push_back(content);
        }
    }
    m_stepping = std::move(stepping);
    m_step = position;
}
void TabView::end_recent_steps()
{
    m_stepping.clear();
}
std::vector<std::size_t> TabView::recent_order() const
{
    std::vector<std::size_t> order;
    if(child_count() == 0)
    {
        return order;
    }
    order.push_back(m_selected);
    for(const auto* const content : m_recent)
    {
        const auto index{index_of(content)};
        if(std::find(order.begin(), order.end(), index) == order.end())
        {
            order.push_back(index);
        }
    }
    for(std::size_t index{}; index < child_count(); ++ index)
    {
        if(std::find(order.begin(), order.end(), index) == order.end())
        {
            order.push_back(index);
        }
    }
    return order;
}
std::optional<std::size_t> TabView::tab_at(double x) const
{
    auto left{bounds().x - m_header_scroll};
    for(std::size_t index{}; index < m_titles.size(); ++ index)
    {
        const auto width{tab_width(index)};
        if(x >= left && x < left + width)
        {
            return index;
        }
        left += width;
    }
    return std::nullopt;
}
void TabView::set_tooltip_provider(std::function<std::optional<std::string>(std::size_t index)> provider)
{
    m_tooltip_provider = std::move(provider);
}
std::optional<std::string> TabView::tooltip_at(Point point) const
{
    if(m_header_visible && whole(point.y - bounds().y) == 0 && m_tooltip_provider)
    {
        if(const auto index{tab_at(point.x)})
        {
            return m_tooltip_provider(*index);
        }
    }
    return Widget::tooltip_at(point);
}
std::unique_ptr<TabView> tab_view()
{
    return std::make_unique<TabView>();
}
TabView& tab_view(Container& parent)
{
    return parent.add(tab_view());
}
} // namespace olagarro::oui
