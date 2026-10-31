#include <oui/command.h>
#include <oui/menu_bar.h>
#include <oui/text_metrics.h>
#include <oui/utf8.h>
#include <oui/text_layout.h>

#include <algorithm>
#include <cctype>
#include <cstdint>

#include "markers.h"

namespace olagarro::oui
{

namespace
{

[[nodiscard]] int text_columns(std::string_view text)
{
    auto columns{0};
    for(std::size_t index{}; index < text.size();)
    {
        columns += std::max(0, terminal_cell_width(decode_utf8(text, index)));
    }
    return columns;
}

// Draws a title with its mnemonic letter underlined, in the hotkey colour unless `hotkey` is empty.
void draw_with_mnemonic(Painter& painter, Point position, std::string_view title, const TextStyle& style,
                        std::optional<Color> hotkey = std::nullopt, Align align = Align::start)
{
    const auto shown{strip_mnemonic(title)};
    if(const auto offset{mnemonic_offset(title)})
    {
        painter.draw_mnemonic_label(position, shown, *offset, style, hotkey.value_or(style.foreground), align);
        return;
    }
    painter.draw_label(position, shown, style, align);
}

[[nodiscard]] bool inside(const Rect& area, double x, double y)
{
    return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
}

} // namespace

MenuItem::MenuItem(std::string command_id) : m_command_id{std::move(command_id)}
{
}
MenuItem::MenuItem(const char* const command_id) : m_command_id{command_id}
{
}
MenuItem::Kind MenuItem::kind() const
{
    return m_kind;
}
const std::string& MenuItem::command_id() const
{
    return m_command_id;
}
const std::string& MenuItem::title() const
{
    return m_title;
}
bool MenuItem::enabled() const
{
    return m_enabled;
}
const std::optional<bool>& MenuItem::checked() const
{
    return m_checked;
}
MenuItem MenuItem::with_shortcut(std::string shortcut) const
{
    auto item{*this};
    item.m_shortcut = std::move(shortcut);
    return item;
}
const std::string& MenuItem::shortcut() const
{
    return m_shortcut;
}
void MenuItem::run() const
{
    if(m_run)
    {
        m_run();
    }
}
std::vector<MenuItem> MenuItem::items() const
{
    return m_items ? m_items() : std::vector<MenuItem>{};
}

MenuItem menu_action(std::string title, std::function<void()> run, bool enabled, std::optional<bool> checked)
{
    MenuItem item;
    item.m_kind = MenuItem::Kind::action;
    item.m_title = std::move(title);
    item.m_run = std::move(run);
    item.m_enabled = enabled;
    item.m_checked = checked;
    return item;
}
MenuItem menu_separator()
{
    MenuItem item;
    item.m_kind = MenuItem::Kind::separator;
    return item;
}
MenuItem menu_submenu(std::string title, MenuItem::Items items)
{
    MenuItem item;
    item.m_kind = MenuItem::Kind::submenu;
    item.m_title = std::move(title);
    item.m_items = std::move(items);
    return item;
}
MenuItem menu_submenu(std::string title, std::vector<MenuItem> items)
{
    return menu_submenu(std::move(title), MenuItem::Items{[items = std::move(items)] { return items; }});
}

MenuBar::MenuBar(CommandRegistry& commands) : m_commands{commands}
{
}

MenuBar::~MenuBar()
{
    for(const auto& menu : m_menus)
    {
        if(!menu.shortcut_command.empty())
        {
            m_commands.unregister_command(menu.shortcut_command);
        }
    }
}

void MenuBar::add_menu(std::string_view title, std::vector<MenuItem> items)
{
    const auto index{m_menus.size()};
    Menu menu{std::string{title}, std::move(items), {}};
    // Alt and the title's mnemonic open the menu from anywhere, through a hidden command.
    if(const auto letter{mnemonic(title)})
    {
        menu.shortcut_command =
            "oui.menu_bar." + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "." + std::to_string(index);
        m_commands.register_command(menu.shortcut_command, strip_mnemonic(title), [this, index] { open_menu(index); });
        m_commands.set_palette_visible(menu.shortcut_command, false);
        m_commands.bind(menu.shortcut_command, Shortcut{letter, false, true});
    }
    m_menus.push_back(std::move(menu));
}

void MenuBar::set_menu_items(std::size_t index, std::vector<MenuItem> items)
{
    if(index >= m_menus.size())
    {
        return;
    }
    m_menus.at(index).items = std::move(items);
    if(m_active == index)
    {
        m_submenu_open = false;
        m_item = std::min(m_item, m_menus.at(index).items.empty() ? 0 : m_menus.at(index).items.size() - 1);
    }
}

void MenuBar::open_menu(std::size_t index)
{
    if(index >= m_menus.size())
    {
        return;
    }
    // Opening a menu shows a bar the application had hidden, as the menu hangs from it.
    set_visible(true);
    m_active = index;
    m_item = 0;
    m_menu_first = 0;
    m_submenu_open = false;
    m_submenu_items.clear();
    if(!m_menus.at(index).items.empty() && row(m_menus.at(index).items.front()).separator)
    {
        move_selection(1);
    }
}

void MenuBar::open_context_menu(std::vector<MenuItem> items, Point at)
{
    if(items.empty())
    {
        return;
    }
    // The bar the menu belongs to shows, as for any other menu.
    set_visible(true);
    m_context_items = std::move(items);
    m_context_at = at;
    m_active = context_menu;
    m_item = 0;
    m_menu_first = 0;
    m_submenu_open = false;
    m_submenu_items.clear();
    if(row(m_context_items.front()).separator)
    {
        move_selection(1);
    }
}

const std::vector<MenuItem>& MenuBar::menu_items() const
{
    return m_active == context_menu ? m_context_items : m_menus.at(m_active).items;
}

double MenuBar::menu_top() const
{
    if(m_active != context_menu)
    {
        return bounds().y + 1;
    }
    // Under the pointer, or as high as it must go to fit above the bottom of the screen.
    const auto height{static_cast<int>(m_context_items.size()) + 2 * text_metrics().popup_frame_rows()};
    // The pointer, in a window, may be between cells: the menu starts at the step.
    return std::max(0.0, snap_y(std::min(m_context_at.y, screen_bottom() - height)));
}

double MenuBar::context_x() const
{
    const Widget* top{this};
    while(top->parent() != nullptr)
    {
        top = top->parent();
    }
    const auto right{top->bounds().x + top->bounds().width};
    return std::max(top->bounds().x, snap_x(std::min(m_context_at.x, right - list_width(m_context_items) - 1)));
}

void MenuBar::close_menu()
{
    m_active = no_menu;
    m_context_items.clear();
    m_submenu_open = false;
    m_submenu_items.clear();
    m_submenu_item = 0;
}

bool MenuBar::is_open() const
{
    return m_active != no_menu;
}

bool MenuBar::is_focusable() const
{
    // An open menu takes the keys as the keyboard target, so the focus stays with what its commands act on, even
    // when an entry is clicked.
    return false;
}

MenuBar::Row MenuBar::row(const MenuItem& item) const
{
    Row result;
    switch(item.kind())
    {
    case MenuItem::Kind::separator:
        result.separator = true;
        break;
    case MenuItem::Kind::command:
        result.title = std::string{m_commands.title(item.command_id()).value_or(item.command_id())};
        if(const auto shortcut{m_commands.shortcut(item.command_id())})
        {
            result.shortcut = format_shortcut(*shortcut);
        }
        result.checked = m_commands.checked(item.command_id());
        result.enabled = m_commands.enabled(item.command_id());
        result.tooltip = std::string{m_commands.tooltip(item.command_id()).value_or("")};
        break;
    case MenuItem::Kind::action:
        result.title = item.title();
        result.shortcut = item.shortcut();
        result.checked = item.checked();
        result.enabled = item.enabled();
        break;
    case MenuItem::Kind::submenu:
        result.title = item.title();
        result.submenu = true;
        break;
    }
    return result;
}

const std::vector<MenuItem>& MenuBar::open_items() const
{
    return m_submenu_open ? m_submenu_items : menu_items();
}

int MenuBar::list_width(const std::vector<MenuItem>& items) const
{
    auto width{5};
    for(const auto& item : items)
    {
        const auto entry{row(item)};
        auto columns{shown_width(strip_mnemonic(entry.title)) + (entry.checked ? 4 : 0) + (entry.submenu ? 2 : 0)};
        if(!entry.shortcut.empty())
        {
            columns += 2 + shown_width(entry.shortcut);
        }
        width = std::max(width, columns + 4);
    }
    return width;
}

double MenuBar::menu_x(std::size_t index) const
{
    auto x{bounds().x};
    for(std::size_t current{}; current < index; ++ current)
    {
        x += shown_width(strip_mnemonic(m_menus.at(current).title)) + 2;
    }
    return x;
}

Rect MenuBar::menu_bounds() const
{
    if(m_active == no_menu)
    {
        return {};
    }
    const auto& items{menu_items()};
    return {m_active == context_menu ? context_x() : menu_x(m_active), menu_top(),
            static_cast<double>(list_width(items)),
            static_cast<double>(menu_layout().rows + 2 * text_metrics().popup_frame_rows())};
}

double MenuBar::screen_bottom() const
{
    const Widget* top{this};
    while(top->parent() != nullptr)
    {
        top = top->parent();
    }
    return top->bounds().y + top->bounds().height;
}

MenuBar::PopupLayout MenuBar::layout_of(std::size_t count, double top, std::size_t first) const
{
    const auto available{std::max(3, whole(screen_bottom() - top) - 2 * text_metrics().popup_frame_rows())};
    PopupLayout layout;
    if(static_cast<int>(count) <= available)
    {
        layout.rows = static_cast<int>(count);
        layout.shown = count;
        return layout;
    }
    layout.arrows = true;
    layout.rows = available;
    layout.shown = static_cast<std::size_t>(available - 2);
    layout.first = std::min(first, count - layout.shown);
    return layout;
}

MenuBar::PopupLayout MenuBar::menu_layout() const
{
    return layout_of(menu_items().size(), menu_top(), m_menu_first);
}

double MenuBar::submenu_top() const
{
    // The first entry of the submenu lines up with the entry that opened it.
    const auto menu{menu_layout()};
    return menu_top() + (menu.arrows ? 1 : 0) + static_cast<int>(m_item) - static_cast<int>(menu.first);
}

MenuBar::PopupLayout MenuBar::submenu_layout() const
{
    return layout_of(m_submenu_items.size(), submenu_top(), m_submenu_first);
}

void MenuBar::reveal_selection()
{
    if(m_active == no_menu)
    {
        return;
    }
    const auto layout{m_submenu_open ? submenu_layout() : menu_layout()};
    auto& first{m_submenu_open ? m_submenu_first : m_menu_first};
    const auto selected{m_submenu_open ? m_submenu_item : m_item};
    first = layout.first;
    if(!layout.arrows)
    {
        first = 0;
    }
    else if(selected < first)
    {
        first = selected;
    }
    else if(selected >= first + layout.shown)
    {
        first = selected - layout.shown + 1;
    }
}

void MenuBar::scroll_list(bool submenu, int step)
{
    const auto layout{submenu ? submenu_layout() : menu_layout()};
    if(!layout.arrows)
    {
        return;
    }
    const auto count{submenu ? m_submenu_items.size() : menu_items().size()};
    auto& first{submenu ? m_submenu_first : m_menu_first};
    first = static_cast<std::size_t>(
        std::clamp(static_cast<int>(layout.first) + step, 0, static_cast<int>(count - layout.shown)));
    // A submenu hangs from an entry of the menu, which has moved.
    if(!submenu)
    {
        m_submenu_open = false;
    }
}

Rect MenuBar::submenu_bounds() const
{
    if(!m_submenu_open)
    {
        return {};
    }
    const auto menu{menu_bounds()};
    const auto width{list_width(m_submenu_items)};
    // Beside the menu on its right, or on its left when the screen ends first.
    const Widget* top{this};
    while(top->parent() != nullptr)
    {
        top = top->parent();
    }
    const auto right_edge{top->bounds().x + top->bounds().width};
    const auto x{menu.x + menu.width + width < right_edge ? menu.x + menu.width : std::max(0.0, menu.x - width)};
    return {x, submenu_top(), static_cast<double>(width),
            static_cast<double>(submenu_layout().rows + 2 * text_metrics().popup_frame_rows())};
}

std::optional<std::string> MenuBar::open_tooltip() const
{
    if(m_active == no_menu)
    {
        return std::nullopt;
    }
    const auto& items{open_items()};
    const auto index{m_submenu_open ? m_submenu_item : m_item};
    if(index >= items.size())
    {
        return std::nullopt;
    }
    const auto tooltip{row(items.at(index)).tooltip};
    return tooltip.empty() ? std::nullopt : std::optional<std::string>{tooltip};
}

Rect MenuBar::paint_bounds() const
{
    if(m_active == no_menu)
    {
        return {bounds().x, bounds().y, bounds().width, 1};
    }
    const auto menu{menu_bounds()};
    auto bottom{menu.y + menu.height};
    auto right{menu.x + menu.width};
    if(m_submenu_open)
    {
        const auto sub{submenu_bounds()};
        bottom = std::max(bottom, sub.y + sub.height);
        right = std::max(right, sub.x + sub.width);
    }
    // One more row and column for the shadow, and a row for the tooltip.
    bottom += 1 + (open_tooltip() ? 1 : 0);
    return {bounds().x, bounds().y, std::max(bounds().width, right + 1 - bounds().x), bottom - bounds().y};
}

bool MenuBar::is_overlay() const
{
    return true;
}

void MenuBar::dismiss_overlays_at(double x, double y)
{
    if(m_active == no_menu)
    {
        return;
    }
    const auto on_bar{x >= bounds().x && x < bounds().x + bounds().width && y == bounds().y};
    if(!on_bar && !inside(menu_bounds(), x, y) && !(m_submenu_open && inside(submenu_bounds(), x, y)))
    {
        close_menu();
    }
}

Widget* MenuBar::hit_test(double x, double y)
{
    const auto on_bar{x >= bounds().x && x < bounds().x + bounds().width && y == bounds().y};
    const auto on_menu{m_active != no_menu && inside(menu_bounds(), x, y)};
    const auto on_sub{m_submenu_open && inside(submenu_bounds(), x, y)};
    return on_bar || on_menu || on_sub ? this : nullptr;
}

Widget* MenuBar::keyboard_target()
{
    return m_active == no_menu ? nullptr : this;
}

void MenuBar::clear()
{
    close_menu();
    m_menus.clear();
}

Size MenuBar::measure(const Constraints& constraints) const
{
    // With no menus there is nothing on it to show; it is there for the menus a right click opens.
    return constraints.constrain({constraints.max_width, m_menus.empty() ? 0.0 : 1.0});
}

void MenuBar::move_selection(int step)
{
    const auto& items{open_items()};
    auto& index{m_submenu_open ? m_submenu_item : m_item};
    if(items.empty())
    {
        return;
    }
    const auto count{static_cast<int>(items.size())};
    auto next{static_cast<int>(index)};
    // Separators are never selected.
    for(auto attempt{0}; attempt < count; ++ attempt)
    {
        next = (next + step + count) % count;
        if(!row(items.at(static_cast<std::size_t>(next))).separator)
        {
            break;
        }
    }
    index = static_cast<std::size_t>(next);
    reveal_selection();
}

bool MenuBar::activate(std::size_t index)
{
    const auto& items{open_items()};
    if(index >= items.size())
    {
        return false;
    }
    const auto item{items.at(index)};
    const auto entry{row(item)};
    if(entry.separator || !entry.enabled)
    {
        return true;
    }
    if(item.kind() == MenuItem::Kind::submenu)
    {
        if(!m_submenu_open)
        {
            m_item = index;
            m_submenu_items = item.items();
            m_submenu_open = true;
            m_submenu_item = 0;
            m_submenu_first = 0;
            if(!m_submenu_items.empty() && row(m_submenu_items.front()).separator)
            {
                move_selection(1);
            }
        }
        return true;
    }
    close_menu();
    if(item.kind() == MenuItem::Kind::command)
    {
        return m_commands.execute(item.command_id());
    }
    item.run();
    return true;
}

bool MenuBar::activate_mnemonic(char32_t letter)
{
    const auto& items{open_items()};
    for(std::size_t index{}; index < items.size(); ++ index)
    {
        if(mnemonic(row(items.at(index)).title) == letter)
        {
            if(m_submenu_open)
            {
                m_submenu_item = index;
            }
            else
            {
                m_item = index;
            }
            return activate(index);
        }
    }
    return false;
}

bool MenuBar::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)})
    {
        return handle_mouse(*mouse);
    }
    if(m_active == no_menu)
    {
        return false;
    }
    if(const auto* const text{std::get_if<TextEvent>(&event)})
    {
        const auto letter{text->codepoint < 0x80
                              ? static_cast<char32_t>(std::tolower(static_cast<int>(text->codepoint)))
                              : text->codepoint};
        if(text->alt)
        {
            for(std::size_t index{}; index < m_menus.size(); ++ index)
            {
                if(mnemonic(m_menus.at(index).title) == letter)
                {
                    open_menu(index);
                    return true;
                }
            }
            return true;
        }
        if(!text->control)
        {
            static_cast<void>(activate_mnemonic(letter));
        }
        return true;
    }
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr)
    {
        return true;
    }
    switch(key->key)
    {
    case Key::escape:
        if(m_submenu_open)
        {
            m_submenu_open = false;
        }
        else
        {
            close_menu();
        }
        return true;
    case Key::down:
        move_selection(1);
        return true;
    case Key::up:
        move_selection(-1);
        return true;
    case Key::enter:
    case Key::space:
        return activate(m_submenu_open ? m_submenu_item : m_item);
    case Key::right:
        if(!m_submenu_open && m_item < menu_items().size() && menu_items().at(m_item).kind() == MenuItem::Kind::submenu)
        {
            return activate(m_item);
        }
        if(m_active != context_menu)
        {
            open_menu((m_active + 1) % m_menus.size());
        }
        return true;
    case Key::left:
        if(m_submenu_open)
        {
            m_submenu_open = false;
            return true;
        }
        if(m_active != context_menu)
        {
            open_menu((m_active + m_menus.size() - 1) % m_menus.size());
        }
        return true;
    default:
        return true;
    }
}

bool MenuBar::handle_mouse(const MouseEvent& mouse)
{
    // The wheel over a list that does not fit scrolls it.
    if(mouse.wheel_delta != 0 && m_active != no_menu)
    {
        const auto over_submenu{m_submenu_open && inside(submenu_bounds(), mouse.x, mouse.y)};
        if(over_submenu || inside(menu_bounds(), mouse.x, mouse.y))
        {
            scroll_list(over_submenu, -mouse.wheel_delta);
            return true;
        }
        return false;
    }
    if(!mouse.pressed && !mouse.hover)
    {
        return false;
    }
    if(whole(mouse.y - bounds().y) == 0 && mouse.x >= bounds().x && mouse.x < bounds().x + bounds().width)
    {
        for(std::size_t index{}; index < m_menus.size(); ++ index)
        {
            const auto x{menu_x(index)};
            if(mouse.x >= x && mouse.x < x + shown_width(strip_mnemonic(m_menus.at(index).title)) + 2)
            {
                if(mouse.pressed)
                {
                    if(m_active == index)
                    {
                        close_menu();
                    }
                    else
                    {
                        open_menu(index);
                    }
                }
                else if(m_active != no_menu && m_active != context_menu && m_active != index)
                {
                    open_menu(index);
                }
                return true;
            }
        }
        return false;
    }
    if(m_active == no_menu)
    {
        return false;
    }
    // Entries of a popup, submenu first since it overlaps the menu. A click on an arrow scrolls the list and is
    // taken; anywhere else outside the entries is none of the list's.
    std::optional<bool> scrolled;
    const auto row_at{[this, &mouse, &scrolled](const Rect& area, const PopupLayout& layout, bool submenu,
                                                std::size_t count) -> std::optional<std::size_t>
                      {
                          const auto frame{text_metrics().popup_frame_rows()};
                          // Not on the columns of its edges, wherever in them the pointer is.
                          if(whole(mouse.x - area.x) <= 0 || whole(mouse.x - (area.x + area.width - 1)) >= 0 ||
                             mouse.y < area.y + frame || mouse.y >= area.y + area.height - frame)
                          {
                              return std::nullopt;
                          }
                          const auto row{whole(mouse.y - area.y) - frame};
                          if(layout.arrows && (row == 0 || row == layout.rows - 1))
                          {
                              if(mouse.pressed)
                              {
                                  scroll_list(submenu, row == 0 ? -1 : 1);
                              }
                              scrolled = true;
                              return std::nullopt;
                          }
                          const auto index{layout.first + static_cast<std::size_t>(row - (layout.arrows ? 1 : 0))};
                          return index < count ? std::optional<std::size_t>{index} : std::nullopt;
                      }};
    if(m_submenu_open)
    {
        if(const auto index{row_at(submenu_bounds(), submenu_layout(), true, m_submenu_items.size())})
        {
            if(row(m_submenu_items.at(*index)).separator)
            {
                return true;
            }
            m_submenu_item = *index;
            return mouse.pressed ? activate(*index) : true;
        }
    }
    if(scrolled)
    {
        return true;
    }
    const auto& items{menu_items()};
    const auto index{row_at(menu_bounds(), menu_layout(), false, items.size())};
    if(!index)
    {
        return scrolled.has_value();
    }
    if(row(items.at(*index)).separator)
    {
        return true;
    }
    if(*index != m_item || !m_submenu_open)
    {
        m_submenu_open = false;
        m_item = *index;
        // Hovering a submenu entry opens it, as clicking does.
        if(items.at(*index).kind() == MenuItem::Kind::submenu)
        {
            return activate(*index);
        }
    }
    // A submenu's entry with it open already: the index is the menu's, not the submenu's, whose entries `activate`
    // would take it among.
    if(items.at(*index).kind() == MenuItem::Kind::submenu)
    {
        return true;
    }
    return mouse.pressed ? activate(*index) : true;
}

void MenuBar::paint_list(Painter& painter, const Theme& theme, const std::vector<MenuItem>& items, const Rect& area,
                         std::size_t selected, const PopupLayout& layout) const
{
    painter.draw_popup(area, theme);
    const auto top{area.y + text_metrics().popup_frame_rows()};
    if(layout.arrows)
    {
        // The arrows that scroll it, dimmed at either end.
        const auto at_start{layout.first == 0};
        const auto at_end{layout.first + layout.shown >= items.size()};
        const Point middle{area.x + std::floor(area.width / 2), top};
        painter.draw_icon(middle, Icon::sorted_up,
                          {at_start ? theme.disabled_foreground : theme.menu_foreground, theme.menu_background});
        painter.draw_icon({middle.x, top + layout.rows - 1}, Icon::sorted_down,
                          {at_end ? theme.disabled_foreground : theme.menu_foreground, theme.menu_background});
    }
    for(auto index{layout.first}; index < layout.first + layout.shown && index < items.size(); ++ index)
    {
        const auto entry{row(items.at(index))};
        const auto y{top + (layout.arrows ? 1 : 0) + static_cast<int>(index - layout.first)};
        if(entry.separator)
        {
            painter.draw_popup_separator({area.x, y, area.width, 1}, theme);
            continue;
        }
        const auto highlighted{index == selected};
        const auto background{highlighted ? theme.menu_selected_background : theme.menu_background};
        const auto foreground{!entry.enabled ? theme.disabled_foreground
                              : highlighted  ? theme.menu_selected_foreground
                                             : theme.menu_foreground};
        painter.fill_rect({area.x + 1, y, area.width - 2, 1}, background);
        auto x{area.x + 2};
        if(entry.checked)
        {
            painter.draw_check_mark({x, y}, *entry.checked, {foreground, background});
            x += text_columns(check_marker(*entry.checked));
        }
        draw_with_mnemonic(painter, {x, y}, entry.title, {foreground, background},
                           highlighted || !entry.enabled ? std::nullopt
                                                         : std::optional<Color>{theme.hotkey_foreground});
        if(entry.submenu)
        {
            painter.draw_icon({area.x + area.width - 3, y}, Icon::submenu, {foreground, background});
        }
        if(!entry.shortcut.empty())
        {
            painter.draw_label({area.x + area.width - 2 - shown_width(entry.shortcut), y}, entry.shortcut,
                               {!entry.enabled ? theme.disabled_foreground
                                : highlighted  ? foreground
                                               : theme.muted_text,
                                background},
                               Align::end);
        }
    }
    painter.draw_popup_edge(area, theme);
}

void MenuBar::paint(Painter& painter, const Theme& theme) const
{
    painter.draw_bar({bounds().x, bounds().y, bounds().width, 1}, BarKind::menu, theme);
    for(std::size_t index{}; index < m_menus.size(); ++ index)
    {
        const auto selected{index == m_active};
        const TextStyle style{selected ? theme.menu_selected_foreground : theme.menu_foreground,
                              selected ? theme.menu_selected_background : theme.menu_background};
        const auto x{menu_x(index)};
        const auto& title{m_menus.at(index).title};
        painter.fill_rect({x, bounds().y, static_cast<double>(shown_width(strip_mnemonic(title)) + 2), 1},
                          style.background);
        draw_with_mnemonic(painter, {x + 1, bounds().y}, title, style,
                           selected ? std::nullopt : std::optional<Color>{theme.hotkey_foreground}, Align::center);
    }
    if(m_active == no_menu)
    {
        return;
    }
    paint_list(painter, theme, menu_items(), menu_bounds(), m_item, menu_layout());
    auto lowest{menu_bounds()};
    if(m_submenu_open)
    {
        paint_list(painter, theme, m_submenu_items, submenu_bounds(), m_submenu_item, submenu_layout());
        lowest = submenu_bounds();
    }
    if(const auto tooltip{open_tooltip()})
    {
        const auto y{lowest.y + lowest.height + 1};
        painter.fill_rect({lowest.x, y, lowest.width, 1}, theme.menu_background);
        painter.draw_label({lowest.x + 1, y}, *tooltip, {theme.menu_foreground, theme.menu_background});
    }
}

std::unique_ptr<MenuBar> menu_bar(CommandRegistry& commands)
{
    return std::make_unique<MenuBar>(commands);
}

MenuBar& menu_bar(CommandRegistry& commands, Container& parent)
{
    return parent.add(menu_bar(commands));
}

} // namespace olagarro::oui
