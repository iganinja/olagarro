#pragma once

#include <oui/command.h>
#include <oui/container.h>
#include <oui/widget.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// One entry of a menu. Titles may mark a mnemonic with '&' ("&Open"); a command's own title is used, and
// may carry one too.
class MenuItem
{
  public:
    enum class Kind
    {
        command,
        action,
        separator,
        submenu
    };
    using Items = std::function<std::vector<MenuItem>()>;

    // A registered command: its title, shortcut, checked and enabled state come from the registry.
    MenuItem(std::string command_id);
    MenuItem(const char* command_id);

    [[nodiscard]] Kind kind() const;
    [[nodiscard]] const std::string& command_id() const;
    [[nodiscard]] const std::string& title() const;
    [[nodiscard]] bool enabled() const;
    [[nodiscard]] const std::optional<bool>& checked() const;
    // The key an action entry shows on its right, such as one a widget handles itself; a command's comes from the
    // registry.
    [[nodiscard]] MenuItem with_shortcut(std::string shortcut) const;
    [[nodiscard]] const std::string& shortcut() const;
    void run() const;
    // A submenu's entries, asked for every time it opens.
    [[nodiscard]] std::vector<MenuItem> items() const;

  private:
    friend MenuItem menu_action(std::string title, std::function<void()> run, bool enabled,
                                std::optional<bool> checked);
    friend MenuItem menu_separator();
    friend MenuItem menu_submenu(std::string title, Items items);
    MenuItem() = default;

    Kind m_kind{Kind::command};
    std::string m_command_id;
    std::string m_title;
    std::function<void()> m_run;
    bool m_enabled{true};
    std::optional<bool> m_checked;
    std::string m_shortcut;
    Items m_items;
};

// An entry that runs a function instead of a registered command, for items built on the fly such as a
// list of recent files.
[[nodiscard]] MenuItem menu_action(std::string title, std::function<void()> run, bool enabled = true,
                                   std::optional<bool> checked = std::nullopt);
[[nodiscard]] MenuItem menu_separator();
[[nodiscard]] MenuItem menu_submenu(std::string title, MenuItem::Items items);
[[nodiscard]] MenuItem menu_submenu(std::string title, std::vector<MenuItem> items);

// The menu bar. A menu title with a mnemonic ("&File") opens with Alt and that letter from anywhere in the
// application; inside an open menu the letters activate the entries. The bar registers those Alt shortcuts
// in `commands` and removes them when it is destroyed, so it must not outlive the registry.
class MenuBar final : public Widget
{
  public:
    explicit MenuBar(CommandRegistry& commands);
    ~MenuBar() override;
    MenuBar(const MenuBar&) = delete;
    MenuBar& operator=(const MenuBar&) = delete;

    void add_menu(std::string_view title, std::vector<MenuItem> items);
    // Takes every menu away, closing the one open.
    void clear();
    void set_menu_items(std::size_t index, std::vector<MenuItem> items);
    void open_menu(std::size_t index);
    // Opens `items` as a menu of their own with its top left corner at `at`, as a right click opens one; it works
    // as the bar's menus do and closes the same way.
    void open_context_menu(std::vector<MenuItem> items, Point at);
    void close_menu();
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] bool is_focusable() const override;
    [[nodiscard]] Rect paint_bounds() const override;
    [[nodiscard]] bool is_overlay() const override;
    void dismiss_overlays_at(double x, double y) override;
    [[nodiscard]] Widget* hit_test(double x, double y) override;
    [[nodiscard]] Widget* keyboard_target() override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    bool handle_event(const Event& event) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    struct Menu
    {
        std::string title;
        std::vector<MenuItem> items;
        std::string shortcut_command;
    };
    // An entry as shown: what the registry says about a command, or the entry's own fields.
    struct Row
    {
        std::string title;
        std::string shortcut;
        std::optional<bool> checked;
        bool enabled{true};
        bool separator{false};
        bool submenu{false};
        std::string tooltip;
    };

    static constexpr std::size_t no_menu{static_cast<std::size_t>(-1)};
    static constexpr std::size_t context_menu{no_menu - 1};

    [[nodiscard]] Row row(const MenuItem& item) const;
    [[nodiscard]] const std::vector<MenuItem>& open_items() const;
    // The entries of the open menu, from the bar or a context menu, without its submenu.
    [[nodiscard]] const std::vector<MenuItem>& menu_items() const;
    [[nodiscard]] double menu_top() const;
    [[nodiscard]] double context_x() const;
    [[nodiscard]] int list_width(const std::vector<MenuItem>& items) const;
    [[nodiscard]] double menu_x(std::size_t index) const;
    [[nodiscard]] Rect menu_bounds() const;
    [[nodiscard]] Rect submenu_bounds() const;
    [[nodiscard]] std::optional<std::string> open_tooltip() const;
    void move_selection(int step);
    // Opens the submenu or runs the entry at `index` of the innermost open list.
    bool activate(std::size_t index);
    [[nodiscard]] bool activate_mnemonic(char32_t letter);
    bool handle_mouse(const MouseEvent& mouse);
    // How a popup's entries fit above the bottom of the screen: when they do not, its first and last rows are
    // arrows that scroll it, and it shows the entries from `first`.
    struct PopupLayout
    {
        int rows{};
        std::size_t first{};
        std::size_t shown{};
        bool arrows{false};
    };
    [[nodiscard]] PopupLayout layout_of(std::size_t count, double top, std::size_t first) const;
    [[nodiscard]] PopupLayout menu_layout() const;
    [[nodiscard]] PopupLayout submenu_layout() const;
    [[nodiscard]] double submenu_top() const;
    [[nodiscard]] double screen_bottom() const;
    // Scrolls the innermost open list so its selected entry shows.
    void reveal_selection();
    void scroll_list(bool submenu, int step);
    void paint_list(Painter& painter, const Theme& theme, const std::vector<MenuItem>& items, const Rect& area,
                    std::size_t selected, const PopupLayout& layout) const;

    CommandRegistry& m_commands;
    std::vector<Menu> m_menus;
    std::size_t m_active{no_menu};
    std::vector<MenuItem> m_context_items;
    Point m_context_at;
    std::size_t m_item{};
    bool m_submenu_open{false};
    std::vector<MenuItem> m_submenu_items;
    std::size_t m_submenu_item{};
    // The first entry each open list shows, when it scrolls.
    std::size_t m_menu_first{};
    std::size_t m_submenu_first{};
};

[[nodiscard]] std::unique_ptr<MenuBar> menu_bar(CommandRegistry& commands);
MenuBar& menu_bar(CommandRegistry& commands, Container& parent);

} // namespace olagarro::oui
