#pragma once

#include <oui/scrollbar.h>
#include <oui/text_search.h>
#include <oui/theme.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace olagarro::oui
{

class Container;

// Markdown as formatted, read-only text that scrolls: headings, paragraphs wrapped at word boundaries,
// lists, block quotes, rules, fenced code and pipe tables, with **strong**, *emphasis*, `code` and links,
// which show their text. An underscore inside a word, as in snake_case, stays an underscore.
// A link is followed by a click, or by Tab to it and Enter. One to a section of the text, "#its-heading", the view
// goes to itself; any other, it says where it leads, for whoever shows it to take there.
class MarkdownView final : public Widget
{
  public:
    // One piece of a shown line and what it is, which decides its colours.
    enum class Role
    {
        text,
        strong,
        emphasis,
        code,
        link,
        heading,
        marker,
        code_block,
        rule,
        table_header
    };
    struct Run
    {
        Run() = default;
        Run(std::string run_text, Role run_role = Role::text) : text{std::move(run_text)}, role{run_role} {}

        std::string text;
        Role role{Role::text};
        // A link's: where it leads, as written, and which link of the text it is, from 0, the same for each piece
        // of one link a line break parts; -1 for the rest.
        std::string target;
        int link{-1};
        // A heading's first piece: its name as a link to its section says it, as GitHub names them: in lower case,
        // spaces as hyphens, other punctuation gone.
        std::string anchor;
    };
    using Line = std::vector<Run>;

    void set_markdown(std::string markdown);
    [[nodiscard]] const std::string& markdown() const;
    // The markdown laid out for `width` columns, as the view shows it.
    [[nodiscard]] std::vector<Line> layout(int width) const;
    // The editor colours of the theme when not set; headings, code and links keep their own.
    void set_background(ColorRef background);
    void set_foreground(ColorRef foreground);
    [[nodiscard]] int scroll_offset() const;
    void scroll_to(int line);
    // Shows the heading `anchor` names at the top; false, staying where it is, when there is none.
    bool scroll_to_anchor(std::string_view anchor);
    // Shows every match of `needle` in the text as laid out with the theme's search colours until cleared, as a find
    // bar does while it is open; a match lies within one line as shown. A new needle or new options choose no match.
    void set_search_highlight(std::string_view needle, const SearchOptions& options = {});
    void clear_search_highlight();
    // How many matches the highlight has, and which of them, counting from 1, is chosen.
    [[nodiscard]] std::size_t search_match_count() const;
    [[nodiscard]] std::optional<std::size_t> selected_search_match() const;
    // Chooses the next or previous match, wrapping round the text, and scrolls to it; with none chosen, the first from
    // the top of the view, or the last before its bottom. The highlight becomes `needle`'s. False when there is none.
    bool find_next(std::string_view needle, const SearchOptions& options = {});
    bool find_previous(std::string_view needle, const SearchOptions& options = {});

    // A link was followed that leads out of the text: where it leads, as written.
    template <typename Callback> olagarro::signal_slot::Connection on_link(Callback&& callback)
    {
        return m_link.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    [[nodiscard]] TextStyle run_style(Role role, const Theme& theme, Color background) const;
    [[nodiscard]] int text_width_available() const;
    [[nodiscard]] int max_scroll() const;
    // The link under the cell at `x`, `y` and `offset` units into it, if any.
    [[nodiscard]] std::optional<int> link_at(double x, double y, int offset) const;
    // Goes where link `link` leads: a section of the text, or out of it, as on_link hears.
    void follow(int link);
    // The first line link `link` shows on.
    [[nodiscard]] std::optional<int> line_of_link(int link) const;

    // Finds the highlight's matches in the lines as laid out.
    void find_search_matches();
    bool find_match(std::string_view needle, const SearchOptions& options, bool forward);
    struct SearchMatch
    {
        int line;
        std::size_t start;
        std::size_t end;
    };

    std::string m_markdown;
    // What the find bar looks for, and where it is in the lines laid out.
    std::optional<TextSearch> m_search;
    std::string m_search_needle;
    SearchOptions m_search_options;
    std::vector<SearchMatch> m_search_matches;
    std::optional<std::size_t> m_selected_search_match;
    std::optional<ColorRef> m_background;
    std::optional<ColorRef> m_foreground;
    // The layout for the width last arranged.
    std::vector<Line> m_lines;
    int m_laid_out_width{-1};
    int m_scroll{};
    // The scrollbar, pressed and dragged.
    ScrollbarDrag m_scrollbar;
    // The link Tab went to, which Enter follows.
    std::optional<int> m_chosen_link;
    olagarro::signal_slot::Signal<std::string> m_link;
};

[[nodiscard]] std::unique_ptr<MarkdownView> markdown_view(std::string markdown = {});
MarkdownView& markdown_view(std::string markdown, Container& parent);

} // namespace olagarro::oui
