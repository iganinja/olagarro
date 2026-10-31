#pragma once

#include <oui/container.h>
#include <oui/syntax_highlighter.h>
#include <oui/text_search.h>

#include <signalslot/signalslot.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// A multi-line UTF-8 text editor with undo, selection, search and optional line numbers and wrapping.
class TextEdit final : public Widget
{
  public:
    void set_text(std::string_view text);
    [[nodiscard]] std::string_view text() const;
    [[nodiscard]] bool is_modified() const;
    // Changes whenever the text does, to tell whether it changed since it was last looked at.
    [[nodiscard]] std::size_t revision() const;
    void set_line_numbers_visible(bool visible);
    // How many spaces Tab inserts when set_use_spaces is on.
    void set_tab_spaces(int spaces);
    // How many columns apart tab stops are when a literal tab is shown; 4 by default, whatever set_tab_spaces
    // says.
    void set_tab_display_width(int columns);
    void set_use_spaces(bool use);
    void set_word_wrap(bool wrap);
    // The vertical scrollbar in the rightmost column; shown by default.
    void set_scrollbar_visible(bool visible);
    // Enter starts the new line with the indentation of the current one; off by default.
    void set_auto_indent(bool enabled);
    // Keeps the text as it is: typing, deleting, pasting and undo do nothing; moving and selecting still work.
    void set_read_only(bool read_only);
    [[nodiscard]] bool is_read_only() const;
    // What is typed and the keys that move and delete go to the editor before the application's shortcuts, as in an
    // application whose shortcuts are single keys, such as a paint program's tools; off by default. Keys with Ctrl
    // that edit nothing here, as Ctrl+S, still reach the shortcuts.
    void set_claims_typing(bool claims);
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_cursor_moved(Callback&& callback)
    {
        return m_cursor_moved.connect(std::forward<Callback>(callback));
    }
    [[nodiscard]] bool word_wrap() const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> line_column() const;
    void mark_saved();
    // For changes that live outside the text, such as the line endings or encoding it will be saved with.
    void mark_modified();
    [[nodiscard]] bool is_focusable() const override;
    [[nodiscard]] std::string selected_text() const;
    // The selection as [first, last) offsets, whichever end the cursor is at; empty at the cursor.
    [[nodiscard]] std::pair<std::size_t, std::size_t> selection_range() const;
    [[nodiscard]] std::size_t cursor_offset() const;
    // The cell on screen the cursor at `offset` is in, as the last arrange placed the text; none when it is
    // scrolled out of sight.
    [[nodiscard]] std::optional<Point> cell_of(std::size_t offset) const;
    void select_range(std::size_t start, std::size_t end);
    void select_all();
    // The word at the cursor, or the run of punctuation or blanks there, as a double click selects it.
    [[nodiscard]] std::pair<std::size_t, std::size_t> word_at_cursor() const;
    void copy_selection(std::string& clipboard) const;
    void cut_selection(std::string& clipboard);
    void paste(std::string_view text);
    // Selects the next or previous match, wrapping around the text.
    [[nodiscard]] bool find_next(std::string_view needle, const SearchOptions& options = {});
    [[nodiscard]] bool find_previous(std::string_view needle, const SearchOptions& options = {});
    [[nodiscard]] bool replace_next(std::string_view needle, std::string_view replacement,
                                    const SearchOptions& options = {});
    // Replaces every match, as one step to undo, and returns how many it replaced. With a regular
    // expression the replacement is plain text, as in replace_next.
    std::size_t replace_all(std::string_view needle, std::string_view replacement, const SearchOptions& options = {});
    // Line operations on the lines the selection touches, or the cursor's line, each one step to undo. They
    // return false when there is nothing to do, such as moving the first line up.
    // Copies the lines below themselves and selects the copy.
    bool duplicate_lines();
    // Swaps the lines with the one above or below, taking the selection along.
    bool move_lines(bool down);
    bool delete_lines();
    // Comments the lines out with `prefix`, such as "//", at the block's indentation; when every line with
    // text already starts with it, takes it off instead.
    bool toggle_line_comment(std::string_view prefix);
    // Shows every match of `needle` with the theme's search colours until cleared, following edits to the
    // text, as a find bar does while it is open.
    void set_search_highlight(std::string_view needle, const SearchOptions& options = {});
    void clear_search_highlight();
    // Marks every other place with the selected text, with the theme's occurrence colours, while the selection is
    // within one line: where else a name is used. Off unless set.
    void set_occurrences_shown(bool shown);
    // How many matches the highlight has, and which of them, counting from 1, is the selection.
    [[nodiscard]] std::size_t search_match_count() const;
    [[nodiscard]] std::optional<std::size_t> selected_search_match() const;
    [[nodiscard]] bool go_to_line(std::size_t line_number);
    // The characters that open and close a pair, such as '(' and ')'; (), [] and {} unless set. The
    // bracket beside the cursor and its pair show in the theme's bracket colours. Every bracket counts,
    // those inside strings and comments too: the editor does not know the language.
    void set_bracket_pairs(std::vector<std::pair<char, char>> pairs);
    // The offsets of the bracket beside the cursor, the one after it first, and of its pair; none when
    // there is no bracket beside the cursor or it has no pair.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> matching_brackets() const;
    // Moves the cursor before the bracket that pairs with the one beside it; false when there is none.
    bool go_to_matching_bracket();
    // Adds a cursor on the line above the topmost cursor, or below the bottommost, at that cursor's column;
    // false when there is no line there. Typing, deleting, moving, copying and pasting then act at every
    // cursor, each change one step to undo. Escape, a click, or a jump such as Go to Line leaves the first
    // cursor alone; line operations act at it alone too.
    bool add_cursor_above();
    bool add_cursor_below();
    // Colours the text by what it is, such as keywords and comments, in the theme's syntax colours; none
    // leaves it all in the editor's colour.
    void set_syntax_highlighter(std::shared_ptr<const SyntaxHighlighter> highlighter);
    // The kinds of a line's text, as the highlighter told them; empty without a highlighter.
    [[nodiscard]] const std::vector<SyntaxSpan>& line_syntax(std::size_t line) const;
    // One, unless cursors were added.
    [[nodiscard]] std::size_t cursor_count() const;
    // Every cursor's selection as (anchor, cursor) offsets, in the order of the text.
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> selections() const;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool claims_key(const Event& event) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    // Fires on_changed and on_cursor_moved, when they apply, as the public call that created it returns.
    class ChangeNotifier
    {
      public:
        explicit ChangeNotifier(TextEdit& editor);
        ~ChangeNotifier();
        ChangeNotifier(const ChangeNotifier&) = delete;
        ChangeNotifier& operator=(const ChangeNotifier&) = delete;

      private:
        TextEdit& m_editor;
        std::size_t m_revision;
        std::size_t m_cursor;
        std::size_t m_anchor;
    };

    bool handle_input(const Event& event);
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> find_matches(std::string_view needle,
                                                                                const SearchOptions& options) const;
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> find_match(std::string_view needle, bool forward,
                                                                                const SearchOptions& options) const;

    // A cursor besides the first, which m_cursor and m_anchor hold.
    struct Caret
    {
        std::size_t cursor;
        std::size_t anchor;
    };
    struct Snapshot
    {
        std::string text;
        std::size_t cursor;
        std::size_t anchor;
        bool is_modified;
        std::vector<Caret> carets;
    };
    // Runs `action`, which acts at m_cursor and m_anchor, once at every cursor in the order of the text,
    // moving the cursors after each change it makes; its changes to the text are one step to undo.
    void at_every_cursor(const std::function<void()>& action);
    bool handle_at_every_cursor(const Event& event);
    bool add_cursor_vertically(int direction);
    // Joins cursors that came to the same place or whose selections overlap, keeping the first cursor.
    void merge_cursors();
    void drop_added_cursors();
    // Every cursor's selection as [first, last) offsets, in the order of the text.
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> selection_ranges() const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> visual_position(std::size_t offset) const;
    struct VisualLine
    {
        std::size_t logical_line;
        std::size_t start_byte;
        std::size_t end_byte;
    };
    // Tab and Shift+Tab over the lines the selection touches, or the cursor's line: one indentation more or
    // less on each, as one step to undo.
    void indent_lines(bool outdent);
    // The first and last line the selection touches; a selection that ends at the start of a line leaves
    // that line out.
    [[nodiscard]] std::pair<std::size_t, std::size_t> selected_lines() const;
    // Replaces [start, end) with `text` as one step to undo, and sets the selection.
    void replace_lines(std::size_t start, std::size_t end, std::string_view text, std::size_t anchor,
                       std::size_t cursor);
    [[nodiscard]] Snapshot snapshot() const;
    void restore(Snapshot state);
    void remember_change();
    void flush_pending_text_change();
    void insert_text(std::string_view text);
    void replace_selection(std::string_view replacement);
    void erase_selection();
    [[nodiscard]] bool erase_before_cursor();
    [[nodiscard]] bool erase_after_cursor();
    [[nodiscard]] bool erase_word(bool forward);
    void move_left(bool extend_selection);
    void move_right(bool extend_selection);
    void move_word(bool forward, bool extend_selection);
    void move_to_line_edge(bool end, bool extend_selection);
    void move_to_first_non_whitespace(bool extend_selection);
    void move_to_last_non_whitespace(bool extend_selection);
    void move_to_document_edge(bool end, bool extend_selection);
    void move_vertical(int offset, bool extend_selection);
    void move_page_vertical(int direction, bool extend_selection);
    [[nodiscard]] std::size_t previous_codepoint(std::size_t index) const;
    [[nodiscard]] const std::vector<std::size_t>& line_starts() const;
    [[nodiscard]] const std::vector<VisualLine>& visual_lines() const;
    void invalidate_visual_lines();
    [[nodiscard]] std::pair<std::size_t, std::size_t> cursor_visual_line() const;
    [[nodiscard]] std::size_t line_end(std::size_t start) const;
    [[nodiscard]] std::pair<std::size_t, std::size_t> cursor_position() const;
    [[nodiscard]] std::size_t column_at_byte(std::size_t start, std::size_t target) const;
    [[nodiscard]] std::size_t byte_at_column(std::size_t start, std::size_t column) const;
    [[nodiscard]] std::size_t char_cell_width(std::size_t index, std::size_t column) const;
    void ensure_cursor_visible();
    [[nodiscard]] int gutter_width() const;
    [[nodiscard]] int content_width() const;
    [[nodiscard]] int scrollbar_width() const;
    [[nodiscard]] std::size_t maximum_scroll_line() const;
    [[nodiscard]] int thumb_height() const;
    [[nodiscard]] int thumb_top() const;
    [[nodiscard]] bool handle_scrollbar(const MouseEvent& mouse);
    [[nodiscard]] int horizontal_scrollbar_height() const;
    [[nodiscard]] int text_rows() const;
    [[nodiscard]] std::size_t content_columns() const;
    [[nodiscard]] std::size_t maximum_scroll_column() const;
    [[nodiscard]] int horizontal_thumb_width() const;
    [[nodiscard]] int horizontal_thumb_left() const;
    [[nodiscard]] bool handle_horizontal_scrollbar(const MouseEvent& mouse);
    // `spans` colour the logical line the visual line belongs to, which starts at `line_start`.
    void paint_line(Painter& painter, const Theme& theme, std::size_t index, std::size_t end, double x, double y,
                    const std::vector<SyntaxSpan>& spans, std::size_t line_start) const;
    // Brings the highlighter's colours up to date down to `line`; lines before it are reused while their text
    // and the state they start in are the same.
    void update_syntax(std::size_t line) const;

    std::string m_text;
    std::size_t m_cursor{};
    std::size_t m_anchor{};
    std::vector<Caret> m_carets;
    std::size_t m_scroll_line{};
    std::size_t m_scroll_column{};
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    std::optional<Snapshot> m_pending_text_change;
    mutable std::vector<std::size_t> m_line_starts;
    mutable bool m_line_starts_dirty{true};
    mutable std::vector<VisualLine> m_visual_lines;
    mutable std::size_t m_visual_lines_width{};
    mutable bool m_visual_lines_dirty{true};
    bool m_is_modified{false};
    bool m_mouse_selecting{false};
    bool m_scrollbar_visible{true};
    bool m_word_or_line_selected{false};
    bool m_auto_indent{false};
    int m_tab_display_width{4};
    bool m_read_only{false};
    bool m_claims_typing{false};
    // Counts text changes, so notifications know whether one happened.
    std::size_t m_revision{};
    // The search highlight, and its matches as of the text revision they were found in.
    // Set when the cursor moved before the editor had a size to scroll in.
    bool m_reveal_cursor_when_arranged{false};
    std::optional<std::pair<std::string, SearchOptions>> m_search_highlight;
    // The places with the selected text, as of the text revision and selection they were found for.
    [[nodiscard]] const std::vector<std::pair<std::size_t, std::size_t>>& occurrences() const;
    bool m_occurrences_shown{false};
    mutable std::vector<std::pair<std::size_t, std::size_t>> m_occurrences;
    mutable std::string m_occurrences_of;
    mutable std::optional<std::size_t> m_occurrences_revision;
    mutable std::vector<std::pair<std::size_t, std::size_t>> m_search_matches;
    mutable std::optional<std::size_t> m_search_matches_revision;
    std::vector<std::pair<char, char>> m_bracket_pairs{{'(', ')'}, {'[', ']'}, {'{', '}'}};
    // matching_brackets() as of the text revision and cursor it was found for.
    struct BracketMatch
    {
        std::size_t revision;
        std::size_t cursor;
        std::optional<std::pair<std::size_t, std::size_t>> brackets;
    };
    mutable std::optional<BracketMatch> m_bracket_match;
    std::shared_ptr<const SyntaxHighlighter> m_highlighter;
    struct HighlightedLine
    {
        std::size_t text_hash;
        SyntaxHighlighter::State start;
        SyntaxHighlighter::State end;
        std::vector<SyntaxSpan> spans;
    };
    mutable std::vector<HighlightedLine> m_highlighted;
    // m_highlighted is known good down to this line as of this revision.
    mutable std::optional<std::pair<std::size_t, std::size_t>> m_highlighted_through;
    [[nodiscard]] std::optional<std::size_t> find_pair(std::size_t offset) const;
    [[nodiscard]] const std::vector<std::pair<std::size_t, std::size_t>>& search_matches() const;
    olagarro::signal_slot::Signal<> m_changed;
    olagarro::signal_slot::Signal<> m_cursor_moved;
    // While the thumb is dragged: the row within the thumb that the pointer grabbed.
    std::optional<int> m_thumb_grab;
    std::optional<int> m_horizontal_thumb_grab;
    mutable std::optional<std::size_t> m_content_columns;
    bool m_mouse_dragging{false};
    bool m_line_numbers_visible{false};
    bool m_use_spaces_for_tab{false};
    int m_tab_spaces{4};
    bool m_word_wrap_enabled{false};
    bool m_last_home_was_at_column_zero{false};
    bool m_last_end_was_at_eol{false};
    std::chrono::steady_clock::time_point m_last_text_change_time{};
};

[[nodiscard]] std::unique_ptr<TextEdit> text_edit();
TextEdit& text_edit(Container& parent);

} // namespace olagarro::oui
