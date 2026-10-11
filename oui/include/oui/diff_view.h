#pragma once

#include <oui/scrollbar.h>
#include <oui/text_diff.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

enum class DiffSide
{
    left,
    right,
};

// Two texts side by side, their lines facing where they are the same and set apart in colour where they are not:
// lines only on the left as removed, only on the right as added, and on both but different as changed, with what
// changed within them marked. A bar over each side names it, and one under both says how many differences there are.
//
// Up and Down move a row at a time, Page Up and Page Down a page, Home and End to the first and last row; Left and
// Right show more of long lines; F7 goes to the next difference and Shift+F7 to the one before. Enter, or a double
// click, says which row and side were chosen, for whoever shows the comparison to go to that line.
class DiffView final : public Widget
{
  public:
    DiffView();

    // Compares the texts again; the row in view stays where it was, as far as the new rows reach.
    void set_texts(std::string left, std::string right);
    // What the bar over each side calls it, as the file's name.
    void set_titles(std::string left, std::string right);
    // How many columns a tab takes; 4 unless set.
    void set_tab_width(int columns);

    [[nodiscard]] const std::vector<DiffRow>& rows() const;
    // How many runs of rows that are not the same there are: removed, added and changed lines next to each other
    // are one difference.
    [[nodiscard]] std::size_t difference_count() const;
    // The row chosen, which the keys move; and the side, which a click picks: the right one unless clicked.
    [[nodiscard]] std::size_t current_row() const;
    [[nodiscard]] DiffSide current_side() const;
    void set_current_row(std::size_t row);
    // To the first row of the next difference, or of the one before; nothing when there is none that way.
    void next_difference();
    void previous_difference();

    // With the side chosen and the line on it, from 0; none when that side has no line on the row.
    template <typename Callback> olagarro::signal_slot::Connection on_activated(Callback&& callback)
    {
        return m_activated.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool claims_key(const Event& event) const override;

  private:
    // Rows of text between the bar over the sides and the one under them.
    [[nodiscard]] int content_rows() const;
    // How wide each side is, and the numbers of its lines.
    [[nodiscard]] int left_width() const;
    [[nodiscard]] int right_width() const;
    [[nodiscard]] int gutter_width() const;
    [[nodiscard]] int text_width() const;
    [[nodiscard]] std::size_t max_scroll_row() const;
    [[nodiscard]] int max_scroll_column() const;
    // Which difference the row is in, from 0.
    [[nodiscard]] std::optional<std::size_t> difference_at(std::size_t row) const;
    void move_to(std::size_t row);
    // The row a difference starts on, chosen and shown near the top.
    void show_difference(std::size_t row);
    void paint_side(Painter& painter, const Theme& theme, const DiffRow& row, DiffSide side, int x, int y, int width,
                    bool current) const;
    [[nodiscard]] int line_columns(std::string_view line) const;

    std::string m_left_text;
    std::string m_right_text;
    std::vector<std::string_view> m_left_lines;
    std::vector<std::string_view> m_right_lines;
    std::vector<DiffRow> m_rows;
    // The first row of each difference.
    std::vector<std::size_t> m_differences;
    std::string m_left_title;
    std::string m_right_title;
    int m_tab_width{4};
    int m_longest_line{};
    std::size_t m_current_row{};
    DiffSide m_current_side{DiffSide::right};
    std::size_t m_scroll_row{};
    // The scrollbar, pressed and dragged.
    ScrollbarDrag m_scrollbar;
    int m_scroll_column{};
    olagarro::signal_slot::Signal<DiffSide, std::optional<std::size_t>> m_activated;
};

[[nodiscard]] std::unique_ptr<DiffView> diff_view();

} // namespace olagarro::oui
