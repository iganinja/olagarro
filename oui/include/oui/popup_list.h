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
    PopupItem() = default;
    // Not an aggregate, so {label, detail} still says all it needs to.
    PopupItem(std::string shown, std::string beside = {}, std::string heading = {})
        : label{std::move(shown)}, detail{std::move(beside)}, section{std::move(heading)}
    {
    }

    // What the list shows and what is chosen.
    std::string label;
    // Beside it, dimmer: what kind of thing it is, or how it is called.
    std::string detail;
    // The heading it is listed under, shown once before the first item of a run with the same one; none for a list
    // without headings.
    std::string section;
};

// Where a popup list opens.
enum class PopupPlacement
{
    // Under its anchor cell, the labels in its column, or over it when there is no room below: suggestions.
    under_anchor,
    // With its top left corner at the anchor, moved left to stay on the screen; with no room below the anchor, its
    // bottom just above it instead, as a menu at the bottom of a window opens upwards: a list at the pointer.
    at_anchor
};

// A list that pops up at a place, as suggestions at the cursor, shown with Application::show_popup: the focus stays
// where it was, typing goes on there, and the list takes only Up and Down, Page Up and Page Down, Enter and Tab to
// choose, and Escape to close. With nothing to list it shows a message instead, as that there is nothing. Its items
// are in the font of the cells, as code is, so a suggestion lines up with the word it would finish. Items with a
// section are listed under it, a heading that is not chosen.
//
// While filtering, it takes what is typed too, and Backspace: the text shows on its first row and only the items
// whose label matches it fuzzily are listed, in their order, the best match selected.
class PopupList final : public Widget
{
  public:
    explicit PopupList(Point anchor);

    // Shows `items`, the first selected.
    void set_items(std::vector<PopupItem> items);
    [[nodiscard]] const std::vector<PopupItem>& items() const;
    // What it says while it has no items.
    void set_message(std::string message);
    // The index in items() of the item selected; what is chosen is told the same way.
    [[nodiscard]] std::size_t selected() const;
    void set_anchor(Point anchor);
    void set_placement(PopupPlacement placement);

    // Whether typing filters it, and what its first row says while nothing is typed.
    void set_filtering(bool filtering, std::string placeholder = {});
    [[nodiscard]] const std::string& filter() const;
    void set_filter(std::string filter);

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
    // A row of the list: an item's index, or a section's heading before it.
    struct Row
    {
        std::size_t item{};
        bool heading{false};
    };

    // Lists again the items the filter keeps, and selects the best of them.
    void refilter();
    [[nodiscard]] int visible_rows() const;
    // The rows above the list's: the filter's.
    [[nodiscard]] int top_rows() const;
    // What it says with nothing listed: its message, or that the filter matches nothing.
    [[nodiscard]] std::string empty_message() const;
    [[nodiscard]] std::size_t row_of_selected() const;
    void move_selection(int by);
    void show_selected();

    Point m_anchor;
    PopupPlacement m_placement{PopupPlacement::under_anchor};
    std::vector<PopupItem> m_items;
    std::string m_message;
    bool m_filtering{false};
    std::string m_placeholder;
    std::string m_filter;
    // The items listed, as indices into m_items, and the rows they take with their headings.
    std::vector<std::size_t> m_shown;
    std::vector<Row> m_rows;
    // The place in m_shown of the item selected.
    std::size_t m_selected{};
    // The first row showing, when there are more than fit.
    std::size_t m_first{};
    olagarro::signal_slot::Signal<std::size_t> m_chosen;
    olagarro::signal_slot::Signal<> m_cancelled;
};

} // namespace olagarro::oui
