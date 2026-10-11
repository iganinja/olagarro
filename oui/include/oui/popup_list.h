#pragma once

#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace olagarro::oui
{

struct PopupItem
{
    // What the list shows and what is chosen.
    std::string label;
    // Beside it, dimmer: what kind of thing it is, or how it is called.
    std::string detail;
};

// A list that pops up at a place, as suggestions at the cursor, shown with Application::show_popup: the focus stays
// where it was, typing goes on there, and the list takes only Up and Down, Page Up and Page Down, Enter and Tab to
// choose, and Escape to close. With nothing to list it shows a message instead, as that there is nothing. Its items
// are in the font of the cells, as code is, so a suggestion lines up with the word it would finish.
class PopupList final : public Widget
{
  public:
    // It opens under `anchor`, the cell it belongs to, or over it when there is no room below.
    explicit PopupList(Point anchor);

    // Shows `items`, the first selected.
    void set_items(std::vector<PopupItem> items);
    [[nodiscard]] const std::vector<PopupItem>& items() const;
    // What it says while it has no items.
    void set_message(std::string message);
    [[nodiscard]] std::size_t selected() const;
    void set_anchor(Point anchor);

    // An item was chosen: its index.
    template <typename Callback> olagarro::signal_slot::Connection on_chosen(Callback&& callback)
    {
        return m_chosen.connect(std::forward<Callback>(callback));
    }
    // Escape was pressed.
    template <typename Callback> olagarro::signal_slot::Connection on_cancelled(Callback&& callback)
    {
        return m_cancelled.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& screen) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;

  private:
    [[nodiscard]] int visible_rows() const;
    void move_selection(int by);

    Point m_anchor;
    std::vector<PopupItem> m_items;
    std::string m_message;
    std::size_t m_selected{};
    // The first item showing, when there are more than fit.
    std::size_t m_first{};
    olagarro::signal_slot::Signal<std::size_t> m_chosen;
    olagarro::signal_slot::Signal<> m_cancelled;
};

} // namespace olagarro::oui
