#pragma once
#include <oui/container.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <signalslot/signalslot.h>
namespace olagarro::oui
{
class TabView final : public Container
{
  public:
    template <typename Child> Child& add_tab(std::string_view title, std::unique_ptr<Child>&& content)
    {
        Child& result{add(std::move(content))};
        m_titles.push_back(std::string{title});
        return result;
    }
    [[nodiscard]] std::size_t selected_index() const;
    [[nodiscard]] std::string_view title(std::size_t index) const;
    void set_title(std::size_t index, std::string_view title);
    // Tabs for which it returns true draw their close mark in the warning colour, such as documents with
    // unsaved changes.
    void set_highlight_predicate(std::function<bool(std::size_t)> predicate);
    // A right click on a tab asks to close it; off by default.
    void set_close_on_right_click(bool enabled);
    // Tabs show a mark that asks to close them; on by default. Off, a tab cannot be closed from its title.
    void set_closable(bool closable);
    void close_tab(std::size_t index);
    // Removes a tab and hands its content over, to be shown elsewhere, such as in another TabView.
    [[nodiscard]] std::unique_ptr<Widget> take_tab(std::size_t index);
    template <typename Type> [[nodiscard]] Type* selected_content()
    {
        return child_count() == 0 ? nullptr : dynamic_cast<Type*>(&child_at(m_selected));
    }
    template <typename Type> [[nodiscard]] const Type* selected_content() const
    {
        return child_count() == 0 ? nullptr : dynamic_cast<const Type*>(&child_at(m_selected));
    }
    template <typename Type> [[nodiscard]] Type* content_at(std::size_t index)
    {
        return index >= child_count() ? nullptr : dynamic_cast<Type*>(&child_at(index));
    }
    void select(std::size_t index);
    // Moves the tab at `from` to `to`, among the others; the same tab stays selected.
    void move_tab(std::size_t from, std::size_t to);
    // Every tab's index, from the selected one through the others as recently as they were selected, as an editor
    // goes back to the document used before.
    [[nodiscard]] std::vector<std::size_t> recent_order() const;
    // The tab whose title covers column `x` of the header, if any.
    [[nodiscard]] std::optional<std::size_t> tab_at(int x) const;
    // What a pointer resting on a tab's title is told about it, such as the whole path of the file it holds.
    void set_tooltip_provider(std::function<std::optional<std::string>(std::size_t index)> provider);
    [[nodiscard]] std::optional<std::string> tooltip_at(Point point) const override;
    // Fired when a tab dragged by its title is let go: which tab, and the cell where. The owner moves it, among
    // these tabs or to another tab view.
    template <typename Callback> olagarro::signal_slot::Connection on_tab_dropped(Callback&& callback)
    {
        return m_tab_dropped.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_close_requested(Callback&& callback)
    {
        return m_close_requested.connect(std::forward<Callback>(callback));
    }
    // Fired whenever a different tab's content becomes the one shown: selecting, arrow keys, or closing.
    template <typename Callback> olagarro::signal_slot::Connection on_selection_changed(Callback&& callback)
    {
        return m_selection_changed.connect(std::forward<Callback>(callback));
    }
    // Fired when the user picks a tab by clicking its title, so the owner can move focus into the content.
    template <typename Callback> olagarro::signal_slot::Connection on_tab_activated(Callback&& callback)
    {
        return m_tab_activated.connect(std::forward<Callback>(callback));
    }
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    void collect_focusable(std::vector<Widget*>& widgets) override;
    [[nodiscard]] Widget* hit_test(int x, int y) override;

  private:
    void ensure_selected_visible();
    [[nodiscard]] std::size_t index_of(const Widget* content) const;
    // The tabs' contents, most recently selected first.
    std::vector<const Widget*> m_recent;
    std::vector<std::string> m_titles;
    std::function<bool(std::size_t)> m_is_highlighted;
    bool m_close_on_right_click{false};
    bool m_closable{true};
    // A tab's title in the header: its text, the spaces round it, and the close mark when there is one.
    [[nodiscard]] int tab_width(std::size_t index) const;
    std::size_t m_selected{};
    int m_header_scroll{};
    olagarro::signal_slot::Signal<std::size_t> m_close_requested;
    olagarro::signal_slot::Signal<std::size_t> m_tab_activated;
    olagarro::signal_slot::Signal<std::size_t> m_selection_changed;
    olagarro::signal_slot::Signal<std::size_t, Point> m_tab_dropped;
    // The tab pressed on its title, while the button is down, and whether it has been dragged off it.
    std::optional<std::size_t> m_pressed_tab;
    bool m_dragging_tab{false};
    std::function<std::optional<std::string>(std::size_t index)> m_tooltip_provider;
};
[[nodiscard]] std::unique_ptr<TabView> tab_view();
TabView& tab_view(Container& parent);
} // namespace olagarro::oui
