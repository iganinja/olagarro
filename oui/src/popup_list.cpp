#include <oui/popup_list.h>

#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/theme.h>
#include <oui/utf8.h>

#include <algorithm>
#include <utility>

namespace olagarro::oui
{

namespace
{

// As many rows as it shows at once; more scroll.
constexpr int most_rows{10};

// How many cells text takes in the cells' font.
[[nodiscard]] int cells_of(std::string_view text)
{
    auto width{0};
    for(std::size_t index{}; index < text.size();)
    {
        width += terminal_cell_width(decode_utf8(text, index));
    }
    return width;
}

} // namespace

PopupList::PopupList(Point anchor) : m_anchor{anchor}
{
}

void PopupList::set_items(std::vector<PopupItem> items)
{
    m_items = std::move(items);
    m_selected = 0;
    m_first = 0;
}

const std::vector<PopupItem>& PopupList::items() const
{
    return m_items;
}

void PopupList::set_message(std::string message)
{
    m_message = std::move(message);
}

std::size_t PopupList::selected() const
{
    return m_selected;
}

void PopupList::set_anchor(Point anchor)
{
    m_anchor = anchor;
}

int PopupList::visible_rows() const
{
    return m_items.empty() ? 1 : std::min(most_rows, static_cast<int>(m_items.size()));
}

Size PopupList::measure(const Constraints& constraints) const
{
    auto inner{m_items.empty() ? shown_width(m_message) : 0};
    for(const auto& item : m_items)
    {
        inner = std::max(inner, cells_of(item.label) + (item.detail.empty() ? 0 : 2 + cells_of(item.detail)));
    }
    // A frame round it, and a cell of room each side inside.
    return constraints.constrain({std::max(10, inner) + 4, visible_rows() + 2});
}

void PopupList::arrange(const Rect& screen)
{
    const auto size{measure({screen.width, screen.height})};
    // The labels start in the anchor's column, the frame and its room before them.
    const auto x{std::clamp(m_anchor.x - 2, screen.x, screen.x + std::max(0, screen.width - size.width))};
    auto y{m_anchor.y + 1};
    if(y + size.height > screen.y + screen.height)
    {
        y = std::max(screen.y, m_anchor.y - size.height);
    }
    Widget::arrange({x, y, size.width, size.height});
}

void PopupList::paint(Painter& painter, const Theme& theme) const
{
    const auto& area{bounds()};
    painter.draw_popup(area, theme);
    const TextStyle normal{theme.menu_foreground, theme.menu_background};
    if(m_items.empty())
    {
        painter.draw_label({area.x + 2, area.y + 1}, m_message,
                           {theme.menu_foreground, theme.menu_background, text_italic});
    }
    for(int row{}; row < visible_rows() && m_first + static_cast<std::size_t>(row) < m_items.size(); ++ row)
    {
        const auto index{m_first + static_cast<std::size_t>(row)};
        const auto& item{m_items[index]};
        const auto y{area.y + 1 + row};
        const auto chosen{index == m_selected};
        const TextStyle style{chosen ? TextStyle{theme.menu_selected_foreground, theme.menu_selected_background}
                                     : normal};
        painter.fill_rect({area.x + 1, y, area.width - 2, 1}, style.background);
        // In the cells' font, as what it suggests is written, so it lines up with the word it would finish.
        painter.draw_text({area.x + 2, y}, item.label, style);
        if(!item.detail.empty())
        {
            painter.draw_text({area.x + area.width - 2 - cells_of(item.detail), y}, item.detail,
                              {style.foreground, style.background, text_dim});
        }
    }
    painter.draw_popup_edge(area, theme);
}

void PopupList::move_selection(int by)
{
    if(m_items.empty())
    {
        return;
    }
    const auto last{static_cast<int>(m_items.size()) - 1};
    m_selected = static_cast<std::size_t>(std::clamp(static_cast<int>(m_selected) + by, 0, last));
    const auto rows{static_cast<std::size_t>(visible_rows())};
    if(m_selected < m_first)
    {
        m_first = m_selected;
    }
    else if(m_selected >= m_first + rows)
    {
        m_first = m_selected + 1 - rows;
    }
}

bool PopupList::handle_event(const Event& event)
{
    if(const auto* const key{std::get_if<KeyEvent>(&event)})
    {
        if(key->control || key->alt || key->shift)
        {
            return false;
        }
        switch(key->key)
        {
        case Key::up:
            move_selection(-1);
            return !m_items.empty();
        case Key::down:
            move_selection(1);
            return !m_items.empty();
        case Key::page_up:
            move_selection(-visible_rows());
            return !m_items.empty();
        case Key::page_down:
            move_selection(visible_rows());
            return !m_items.empty();
        case Key::enter:
        case Key::tab:
            if(m_items.empty())
            {
                return false;
            }
            m_chosen.fire(m_selected);
            return true;
        case Key::escape:
            m_cancelled.fire();
            return true;
        default:
            return false;
        }
    }
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        if(mouse->wheel_delta != 0)
        {
            move_selection(-mouse->wheel_delta);
            return true;
        }
        const auto row{mouse->y - bounds().y - 1};
        if(row < 0 || row >= visible_rows() || m_first + static_cast<std::size_t>(row) >= m_items.size())
        {
            return true;
        }
        m_selected = m_first + static_cast<std::size_t>(row);
        if(mouse->pressed && !mouse->right)
        {
            m_chosen.fire(m_selected);
        }
        return true;
    }
    return false;
}

} // namespace olagarro::oui
