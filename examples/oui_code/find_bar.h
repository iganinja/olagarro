#pragma once

#include <oui/application.h>
#include <oui/check_button.h>
#include <oui/container.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/text_edit.h>
#include <oui/text_search.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <memory>
#include <vector>

// The find bar under the documents, Ctrl+F: it finds in the document on show as one types, lights up and counts the
// matches, and goes from one to the next. Its options open over it.
class FindBar final : public olagarro::oui::Container
{
  public:
    FindBar(olagarro::oui::Application& application, olagarro::oui::TextEdit& document);
    ~FindBar() override;
    FindBar(const FindBar&) = delete;
    FindBar& operator=(const FindBar&) = delete;

    void open(olagarro::oui::TextEdit& document);
    // The tab the user switched to: the matches move with it while the bar is open.
    void set_document(olagarro::oui::TextEdit& document);
    void close();
    void next_match();
    void previous_match();
    // Ctrl+F3: what is selected in `document`, or the word at its cursor, becomes what is looked for, as plain
    // text, and the next place with it is selected; F3 and Shift+F3 go on from there.
    void find_selected(olagarro::oui::TextEdit& document);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    [[nodiscard]] olagarro::oui::Rect paint_bounds() const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    olagarro::oui::Widget* hit_test(int x, int y) override;
    // The options, while open, spill over the documents above the bar, so the bar paints after them.
    [[nodiscard]] bool is_overlay() const override;
    void dismiss_overlays_at(int x, int y) override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    bool focus_option(int offset);
    // Opens the options, the focus on the first, or closes them, the focus back on what is looked for.
    void toggle_options();
    [[nodiscard]] olagarro::oui::Rect options_bounds() const;
    [[nodiscard]] olagarro::oui::SearchOptions search_options() const;
    // Selects the first match from where the search started; with none, such as after a typo, everything stays
    // where it was rather than jump.
    void find_as_typed();
    // Whether there was a match to go to.
    bool find_match(bool forward);
    // Follows the document's edits and selection, to keep the count right.
    void watch(olagarro::oui::TextEdit& document);
    // Shows the query's matches in the document while the bar is open.
    void refresh();
    void update_count();
    // The document it finds in, while it is still there.
    [[nodiscard]] olagarro::oui::TextEdit* shown_document() const;

    olagarro::oui::Application& m_application;
    // The document it finds in, which may close: another may be made where it was, and is not it.
    olagarro::oui::WidgetRef<olagarro::oui::TextEdit> m_document;
    olagarro::oui::LineEdit* m_query{};
    olagarro::oui::Label* m_count{};
    // Where the cursor was when the bar opened, which typing finds from.
    std::size_t m_search_start{};
    std::vector<olagarro::signal_slot::Connection> m_document_connections;
    olagarro::oui::Column* m_options_panel{};
    olagarro::oui::CheckButton* m_whole_word_button{};
    olagarro::oui::CheckButton* m_regex_button{};
    olagarro::oui::CheckButton* m_match_case_button{};
    bool m_options_open{false};
    bool m_whole_word{false};
    bool m_regex{false};
    bool m_match_case{false};
};
