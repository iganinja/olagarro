#pragma once

#include <oui/container.h>
#include <oui/scrollbar.h>
#include <oui/theme.h>
#include <oui/type_ahead.h>

#include <signalslot/signalslot.h>

#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace olagarro::oui
{

enum class SortOrder
{
    ascending,
    descending,
    none
};

namespace detail
{
struct PairHash
{
    std::size_t operator()(const std::pair<std::size_t, std::size_t>& p) const noexcept
    {
        return std::hash<std::size_t>{}(p.first) ^ (std::hash<std::size_t>{}(p.second) << 1);
    }
};
} // namespace detail

// Rows of cells the user scrolls through and picks from. Every child is a cell, in row order, and a cell can
// be any widget: text, a check button, an icon. Selection, the keyboard, typing and scrolling go by rows.
// Lists and other common shapes are Tables configured for the purpose, such as List.
class Table : public Container
{
  public:
    Table();

    // === Columns ===
    // The number of columns: as many as have a width or, failing that, as many headers as show; one when
    // nothing says otherwise.
    [[nodiscard]] std::size_t column_count() const;
    // A column's width, in terminal cells.
    void set_column_width(std::size_t col, int width_cells);
    [[nodiscard]] int column_width(std::size_t col) const;
    // The column that takes whatever width the others leave.
    void set_stretch_column(std::optional<std::size_t> col);
    void set_gap(Spacing gap);

    // === Cells ===
    // Adds a row of text cells.
    void add_line(std::initializer_list<std::string_view> cells);
    void set_cell_widget(std::size_t row, std::size_t col, std::unique_ptr<Widget> widget);
    [[nodiscard]] Widget* cell_widget(std::size_t row, std::size_t col);
    void clear();

    // === Rows and selection ===
    [[nodiscard]] std::size_t row_count() const;
    // With nothing selected, the first row stands as the selection while the table has the focus.
    [[nodiscard]] std::optional<std::size_t> selected_row() const;
    // Every selection goes through here, from keys, clicks or typing, and scrolls the row into view; a
    // subclass can follow the selection by overriding it.
    virtual void select_row(std::size_t row);
    // Fires with the row whenever the selection changes.
    template <typename Callback> olagarro::signal_slot::Connection on_selection_changed(Callback&& callback)
    {
        return m_selection_changed.connect(std::forward<Callback>(callback));
    }
    // Fires with the row on Enter or a double click.
    template <typename Callback> olagarro::signal_slot::Connection on_row_activated(Callback&& callback)
    {
        return m_row_activated.connect(std::forward<Callback>(callback));
    }
    // Typing selects the first row whose text starts with what was typed, as file managers do; `text_of`
    // says what a row reads as, which can differ from what it shows.
    void set_type_ahead(std::function<std::string(std::size_t row)> text_of);

    // === Scrolling ===
    [[nodiscard]] int scroll_offset() const;
    // Shows the rows from `offset` down, as far as there are rows to show: where a table filled again was.
    void scroll_to(int offset);
    // The row shown at row `y` of the screen, if any: none on the headers or below the last row.
    [[nodiscard]] std::optional<std::size_t> row_at(int y) const;
    void ensure_row_visible(std::size_t row);
    [[nodiscard]] int scroll_offset_x() const;
    void scroll_to_x(int offset);
    // Left and Right scroll sideways; the table also turns it on when its columns are wider than it.
    void set_horizontal_scroll_enabled(bool enabled);
    [[nodiscard]] bool horizontal_scroll_enabled() const;

    // === Headers ===
    // Setting headers shows them.
    void set_headers(std::vector<std::string> headers);
    [[nodiscard]] const std::vector<std::string>& headers() const { return m_headers; }
    void set_headers_visible(bool visible);
    [[nodiscard]] bool headers_visible() const;
    // A click on a header cycles its column through ascending, descending and unsorted, when let go on it.
    void set_headers_clickable(bool clickable);
    [[nodiscard]] bool headers_clickable() const;
    // Dragging the edge between two headers makes the column on its left wider or narrower, or for the stretch
    // column the one on its right; off by default.
    void set_columns_resizable(bool resizable);
    // Fires with the column and its new width when a drag on its edge ends.
    template <typename Callback> olagarro::signal_slot::Connection on_column_resized(Callback&& callback)
    {
        return m_column_resized.connect(std::forward<Callback>(callback));
    }
    // A click on a header only says so, with the order it asks for, and the owner sorts the rows and marks the
    // column with set_column_sort: for rows the owner makes from data of its own.
    void set_sorted_by_owner(bool by_owner);

    // === Sorting ===
    using CompareFunction = std::function<int(const Widget*, const Widget*)>;
    void set_column_sort(std::size_t col, SortOrder order, CompareFunction custom_compare = nullptr);
    [[nodiscard]] SortOrder column_sort_order(std::size_t col) const;
    [[nodiscard]] std::size_t sort_column_count() const;
    // Reorders the rows by the column, text cells comparing their text unless the column has its own compare.
    void apply_sort(std::size_t col);
    // Fires when a header click sorts a column, with the column and its new order.
    template <typename Callback> olagarro::signal_slot::Connection on_column_click(Callback&& callback)
    {
        return m_column_clicked.connect(std::forward<Callback>(callback));
    }

    // === Per-cell metadata, kept with the row when the table is sorted ===
    using MetadataValue =
        std::variant<std::nullptr_t, int, bool, float, double, std::string, std::vector<int>, std::vector<std::string>>;
    void set_metadata(std::size_t row, std::size_t col, std::string_view key, MetadataValue value);
    [[nodiscard]] const MetadataValue* metadata(std::size_t row, std::size_t col, std::string_view key) const;
    template <typename T>
    [[nodiscard]] T metadata_or(std::size_t row, std::size_t col, std::string_view key, T default_value) const
    {
        const auto* const value{metadata(row, col, key)};
        if(value != nullptr && std::holds_alternative<T>(*value))
        {
            return std::get<T>(*value);
        }
        return default_value;
    }
    void clear_metadata(std::size_t row, std::size_t col);
    void clear_all_metadata();

    // The colours of its rows of text, and of the room around them: the theme's table colours by default. Always
    // both, as the text must read on the background.
    void set_colors(ColorRef background, ColorRef foreground);

    [[nodiscard]] bool is_focusable() const override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    bool handle_event(const Event& event) override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    [[nodiscard]] bool is_selected_child(const Widget& child) const override;

  protected:
    // The width a column gets in the current bounds, the stretch column taking what the others leave.
    [[nodiscard]] int column_pixel_width(std::size_t col) const;
    [[nodiscard]] int header_pixel_x(std::size_t col) const;
    [[nodiscard]] int header_height() const;
    // The colour behind the rows: the table's background, or the dialog's when it has none.
    [[nodiscard]] Color background_color(const Theme& theme) const;

  private:
    struct ColumnSort
    {
        bool enabled{false};
        SortOrder order{SortOrder::ascending};
        CompareFunction compare;
    };

    [[nodiscard]] int max_scroll_offset() const;
    [[nodiscard]] int max_scroll_offset_x() const;
    // Whether `row` shows as selected: the selection, or the first row while focused with none.
    [[nodiscard]] bool highlights_row(std::size_t row) const;
    void paint_scrollbar(Painter& painter, const Theme& theme) const;
    void paint_horizontal_scrollbar(Painter& painter, const Theme& theme) const;
    void paint_headers(Painter& painter, const Theme& theme) const;
    // Sorts by the column, the next way round, as a click on its header does.
    void click_header(std::size_t col);
    // The column whose right edge is at `x`, within a few pixels, on the header row.
    [[nodiscard]] std::optional<std::size_t> edge_at(int x, int cell_offset) const;
    // Handles the mouse on the headers: a press, then a drag that resizes or a release that sorts.
    [[nodiscard]] bool handle_header_mouse(const MouseEvent& mouse);

    std::vector<int> m_column_widths;
    std::optional<std::size_t> m_stretch_column;
    Spacing m_gap{Spacing::none};
    ColorRef m_background{&Theme::table_background};
    ColorRef m_foreground{&Theme::table_foreground};

    std::optional<std::size_t> m_selected_row;
    olagarro::signal_slot::Signal<std::size_t> m_selection_changed;
    olagarro::signal_slot::Signal<std::size_t> m_row_activated;
    std::function<std::string(std::size_t row)> m_type_ahead_text;
    TypeAhead m_type_ahead;

    int m_scroll_offset{};

    // The scrollbar, pressed and dragged.

    ScrollbarDrag m_scrollbar;
    int m_viewport_height{};
    int m_total_height{};
    int m_scroll_offset_x{};
    int m_viewport_width{};
    int m_total_width{};
    bool m_horizontal_scroll_enabled{false};
    // Where each row starts in the content and how tall it is, as last arranged.
    std::vector<int> m_row_tops;
    std::vector<int> m_row_heights;
    // A row selected before the table had a size, to scroll into view once it has one.
    std::optional<std::size_t> m_reveal_row;

    std::vector<std::string> m_headers;
    bool m_headers_visible{false};
    bool m_headers_clickable{false};
    bool m_sorted_by_owner{false};
    // The header held down, until the button is let go.
    std::optional<std::size_t> m_pressed_header;
    bool m_columns_resizable{false};
    // A column being resized by its edge: which, which way the pointer widens it, and where the drag started.
    struct ColumnDrag
    {
        std::size_t col{};
        int direction{1};
        int start_x{};
        int start_width{};
        bool moved{false};
    };
    std::optional<ColumnDrag> m_column_drag;
    olagarro::signal_slot::Signal<std::size_t, int> m_column_resized;
    std::vector<ColumnSort> m_sort_specs;
    olagarro::signal_slot::Signal<std::size_t, SortOrder> m_column_clicked;
    // {row, col} -> {key, value}
    std::unordered_map<std::pair<std::size_t, std::size_t>, std::unordered_map<std::string, MetadataValue>,
                       detail::PairHash>
        m_metadata;
};

[[nodiscard]] std::unique_ptr<Table> table(std::vector<std::string> headers = {});
Table& table(std::vector<std::string> headers, Container& parent);

} // namespace olagarro::oui
