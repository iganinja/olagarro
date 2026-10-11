#pragma once

#include <oui/theme.h>

#include <oui/container.h>

#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <optional>
#include <list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// Nodes live in lists so that adding one never moves the others: the reference add returns stays valid.
struct TreeNode
{
    std::string text;
    std::list<TreeNode> children;
    bool expanded{false};

    TreeNode& add(std::string_view child_text);
};

class TreeView final : public Widget
{
  public:
    TreeNode& add(std::string_view text);
    void clear();
    void set_background(ColorRef color);
    void set_empty_message(std::string message);
    void set_show_expanders(bool enabled);
    void set_indentation(int cells);
    void set_top_padding(int rows);
    // A title over the entries, in the first row the top padding leaves, as a table's column headers show.
    void set_header(std::string header);
    void set_focus_proxy_active(bool active);
    [[nodiscard]] bool is_focusable() const override;

    template <typename Callback> olagarro::signal_slot::Connection on_activated(Callback&& callback)
    {
        return m_activated.connect(std::forward<Callback>(callback));
    }
    // The nodes at the top, each with its own under it.
    [[nodiscard]] const std::list<TreeNode>& nodes() const;
    // The node chosen, if any.
    [[nodiscard]] const TreeNode* selected() const;
    // Chooses `node`, showing it: what is above it opens, as far as it is in the tree.
    void select(const TreeNode& node);
    // Another node was chosen, with the keys or the mouse.
    template <typename Callback> olagarro::signal_slot::Connection on_selection_changed(Callback&& callback)
    {
        return m_selection_changed.connect(std::forward<Callback>(callback));
    }

    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    struct Entry
    {
        TreeNode* node;
        int depth;
    };

    struct ConstEntry
    {
        const TreeNode* node;
        int depth;
    };

    void append_visible(std::vector<Entry>& entries, TreeNode& node, int depth);
    [[nodiscard]] std::vector<Entry> visible_nodes();
    void append_visible(std::vector<ConstEntry>& entries, const TreeNode& node, int depth) const;
    [[nodiscard]] std::vector<ConstEntry> visible_nodes_const() const;
    [[nodiscard]] std::size_t entry_height(const ConstEntry& entry, int available_width) const;
    [[nodiscard]] std::size_t visual_height(const std::vector<ConstEntry>& entries, int available_width) const;
    [[nodiscard]] int content_view_width(const std::vector<ConstEntry>& entries) const;
    [[nodiscard]] std::size_t scrollable_visual_height(const std::vector<ConstEntry>& entries) const;
    [[nodiscard]] std::optional<std::size_t> entry_at_visual_row(const std::vector<ConstEntry>& entries,
                                                                 std::size_t row, int available_width) const;
    [[nodiscard]] std::size_t visual_row_of_entry(const std::vector<ConstEntry>& entries, std::size_t index,
                                                  int available_width) const;
    void select_first_visible(const std::vector<ConstEntry>& entries);
    [[nodiscard]] std::size_t visible_height() const;
    [[nodiscard]] std::size_t maximum_scroll_offset(std::size_t visual_row_count) const;
    void ensure_selection_visible(const std::vector<ConstEntry>& entries);
    // handle_event, but for saying when the node chosen changed.
    bool handle_event_as_is(const Event& event);

    std::list<TreeNode> m_nodes;
    std::size_t m_selected{0};
    std::size_t m_scroll_offset{};
    // The theme's panel background when not set.
    std::optional<ColorRef> m_background;
    std::string m_empty_message;
    olagarro::signal_slot::Signal<std::size_t> m_activated;
    olagarro::signal_slot::Signal<> m_selection_changed;
    int m_drag_y{};
    int m_press_y{};
    int m_scroll_grab{};
    bool m_dragging_scrollbar{false};
    bool m_show_expanders{true};
    int m_indentation{2};
    int m_top_padding{};
    std::string m_header;
    bool m_focus_proxy_active{false};
};

[[nodiscard]] std::unique_ptr<TreeView> tree_view();
TreeView& tree_view(Container& parent);

} // namespace olagarro::oui
