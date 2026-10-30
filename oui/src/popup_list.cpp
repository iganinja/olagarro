#include <oui/popup_list.h>

#include <oui/fuzzy.h>
#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/theme.h>
#include <oui/translation.h>
#include <oui/utf8.h>

#include <algorithm>
#include <optional>
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
    refilter();
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
    return m_shown.empty() ? 0 : m_shown[m_selected];
}

void PopupList::set_anchor(Point anchor)
{
    m_anchor = anchor;
}

void PopupList::set_placement(PopupPlacement placement)
{
    m_placement = placement;
}

void PopupList::set_filtering(bool filtering, std::string placeholder)
{
    m_filtering = filtering;
    m_placeholder = placeholder.empty() ? translate("PopupList", "Type to filter") : std::move(placeholder);
    m_filter.clear();
    refilter();
}

const std::string& PopupList::filter() const
{
    return m_filter;
}

void PopupList::set_filter(std::string filter)
{
    m_filter = std::move(filter);
    refilter();
}

void PopupList::refilter()
{
    m_shown.clear();
    m_rows.clear();
    m_selected = 0;
    m_first = 0;
    std::optional<int> best;
    for(std::size_t index{}; index < m_items.size(); ++index)
    {
        const auto& item{m_items[index]};
        if(m_filtering && !m_filter.empty())
        {
            const auto score{fuzzy_score(m_filter, item.label)};
            if(!score)
            {
                continue;
            }
            if(!best || *score > *best)
            {
                best = score;
                m_selected = m_shown.size();
            }
        }
        if(!item.section.empty() && (m_shown.empty() || m_items[m_shown.back()].section != item.section))
        {
            m_rows.push_back({index, true});
        }
        m_shown.push_back(index);
        m_rows.push_back({index, false});
    }
    show_selected();
}

std::string PopupList::empty_message() const
{
    return m_filtering && !m_filter.empty() && !m_items.empty() ? translate("PopupList", "Nothing matches") : m_message;
}

int PopupList::top_rows() const
{
    return m_filtering ? 1 : 0;
}

int PopupList::visible_rows() const
{
    return m_rows.empty() ? 1 : std::min(most_rows, static_cast<int>(m_rows.size()));
}

std::size_t PopupList::row_of_selected() const
{
    for(std::size_t row{}; row < m_rows.size(); ++row)
    {
        if(!m_rows[row].heading && m_rows[row].item == m_shown[m_selected])
        {
            return row;
        }
    }
    return 0;
}

Size PopupList::measure(const Constraints& constraints) const
{
    // As wide as every item, not only those the filter keeps, so it does not change width as one types.
    auto inner{m_shown.empty() ? shown_width(empty_message()) : 0};
    if(m_filtering)
    {
        inner = std::max({inner, cells_of(m_filter) + 1, shown_width(m_placeholder)});
    }
    for(const auto& item : m_items)
    {
        inner = std::max({inner, cells_of(item.label) + (item.detail.empty() ? 0 : 2 + cells_of(item.detail)),
                          shown_width(item.section) - 1});
    }
    // A frame round it, and a cell of room each side inside.
    return constraints.constrain({std::max(10, inner) + 4, top_rows() + visible_rows() + 2});
}

void PopupList::arrange(const Rect& screen)
{
    const auto size{measure({screen.width, screen.height})};
    const auto right{screen.x + std::max(0, screen.width - size.width)};
    const auto bottom{screen.y + std::max(0, screen.height - size.height)};
    if(m_placement == PopupPlacement::at_anchor)
    {
        auto y{m_anchor.y};
        if(y + size.height > screen.y + screen.height)
        {
            y = std::max(screen.y, m_anchor.y - size.height);
        }
        Widget::arrange(
            {std::clamp(m_anchor.x, screen.x, right), std::clamp(y, screen.y, bottom), size.width, size.height});
        return;
    }
    // The labels start in the anchor's column, the frame and its room before them.
    const auto x{std::clamp(m_anchor.x - 2, screen.x, right)};
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
    if(m_filtering)
    {
        if(m_filter.empty())
        {
            painter.draw_label({area.x + 2, area.y + 1}, m_placeholder,
                               {theme.menu_foreground, theme.menu_background, text_italic | text_dim});
        }
        else
        {
            painter.draw_text({area.x + 2, area.y + 1}, m_filter, normal);
        }
    }
    const auto top{area.y + 1 + top_rows()};
    if(m_shown.empty())
    {
        painter.draw_label({area.x + 2, top}, empty_message(),
                           {theme.menu_foreground, theme.menu_background, text_italic});
    }
    for(int row{}; row < visible_rows() && m_first + static_cast<std::size_t>(row) < m_rows.size(); ++row)
    {
        const auto& shown{m_rows[m_first + static_cast<std::size_t>(row)]};
        const auto& item{m_items[shown.item]};
        const auto y{top + row};
        if(shown.heading)
        {
            painter.fill_rect({area.x + 1, y, area.width - 2, 1}, normal.background);
            painter.draw_label({area.x + 1, y}, item.section,
                               {theme.menu_foreground, theme.menu_background, text_bold | text_dim});
            continue;
        }
        const auto chosen{!m_shown.empty() && shown.item == m_shown[m_selected]};
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

void PopupList::show_selected()
{
    if(m_shown.empty())
    {
        m_first = 0;
        return;
    }
    const auto rows{static_cast<std::size_t>(visible_rows())};
    auto row{row_of_selected()};
    // The heading over the item shows with it.
    if(row > 0 && m_rows[row - 1].heading)
    {
        --row;
    }
    if(row < m_first)
    {
        m_first = row;
    }
    else if(row_of_selected() >= m_first + rows)
    {
        m_first = row_of_selected() + 1 - rows;
    }
}

void PopupList::move_selection(int by)
{
    if(m_shown.empty())
    {
        return;
    }
    const auto last{static_cast<int>(m_shown.size()) - 1};
    m_selected = static_cast<std::size_t>(std::clamp(static_cast<int>(m_selected) + by, 0, last));
    show_selected();
}

bool PopupList::handle_event(const Event& event)
{
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        if(!m_filtering || text->control || text->alt || text->codepoint < U' ')
        {
            return false;
        }
        m_filter += encode_utf8(text->codepoint);
        refilter();
        return true;
    }
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
            return !m_shown.empty();
        case Key::down:
            move_selection(1);
            return !m_shown.empty();
        case Key::page_up:
            move_selection(-visible_rows());
            return !m_shown.empty();
        case Key::page_down:
            move_selection(visible_rows());
            return !m_shown.empty();
        case Key::enter:
        case Key::tab:
            if(m_shown.empty())
            {
                return m_filtering;
            }
            m_chosen.fire(m_shown[m_selected]);
            return true;
        case Key::backspace:
            if(!m_filtering)
            {
                return false;
            }
            if(!m_filter.empty())
            {
                // The last character, whole.
                auto end{m_filter.size() - 1};
                while(end > 0 && (static_cast<unsigned char>(m_filter[end]) & 0xc0u) == 0x80u)
                {
                    --end;
                }
                m_filter.erase(end);
                refilter();
            }
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
        const auto row{mouse->y - bounds().y - 1 - top_rows()};
        if(row < 0 || row >= visible_rows() || m_first + static_cast<std::size_t>(row) >= m_rows.size())
        {
            return true;
        }
        const auto& shown{m_rows[m_first + static_cast<std::size_t>(row)]};
        if(shown.heading)
        {
            return true;
        }
        m_selected = static_cast<std::size_t>(std::find(m_shown.begin(), m_shown.end(), shown.item) - m_shown.begin());
        if(mouse->pressed && !mouse->right)
        {
            m_chosen.fire(shown.item);
        }
        return true;
    }
    return false;
}

} // namespace olagarro::oui
