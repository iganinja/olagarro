#pragma once

#include "live_script.h"

#include <oui/button.h>
#include <oui/container.h>
#include <oui/label.h>
#include <oui/split_view.h>
#include <oui/text_edit.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

// A line that says what went wrong, read from its start, which a click takes to where it went wrong.
class ErrorLine final : public olagarro::oui::Widget
{
  public:
    explicit ErrorLine(std::function<void()> clicked);
    void set_text(std::string text);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    std::function<void()> m_clicked;
    std::string m_text;
};

// A script and what it gives, in one tab, as Tools > Interactive Script makes it: the code on the left, what it
// made of its input on the right, what it printed under that, and why it failed along the bottom, when it does.
// The code is a document as any other; the rest only shows, but for the text it runs on when that is pasted in the
// tab, over the result. After a failure what it made last stays, marked as such, until it works again.
class InteractiveView final : public olagarro::oui::Container
{
  public:
    explicit InteractiveView(std::unique_ptr<olagarro::oui::TextEdit> code);

    [[nodiscard]] olagarro::oui::TextEdit& code();
    // Shows a run's result; `input_name` names what it ran on, as the result's title says.
    void show(const LiveResult& result, std::string_view input_name);
    // The result, as Apply puts it in the input; none when the last run gave no text.
    [[nodiscard]] const std::optional<std::string>& result() const;
    void set_log_visible(bool visible);
    // The text pasted in the tab, for the script to run on; it shows once given.
    [[nodiscard]] olagarro::oui::TextEdit& input();
    void show_input(std::string_view text);
    // Whether the result can go in place of what it ran on, as Apply puts it, as it can unless it says something
    // else, such as how running after Generate would go.
    void set_applies(bool applies);
    [[nodiscard]] bool log_visible() const;

    // Apply was pressed.
    template <typename Callback> olagarro::signal_slot::Connection on_apply(Callback&& callback)
    {
        return m_apply.connect(std::forward<Callback>(callback));
    }
    // The error was clicked: its line of the code, from 1.
    template <typename Callback> olagarro::signal_slot::Connection on_error_chosen(Callback&& callback)
    {
        return m_error_chosen.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    olagarro::oui::TextEdit* m_code{};
    olagarro::oui::Label* m_result_title{};
    olagarro::oui::Button* m_apply_button{};
    olagarro::oui::TextEdit* m_result_text{};
    olagarro::oui::Widget* m_log_pane{};
    olagarro::oui::TextEdit* m_log_text{};
    olagarro::oui::Widget* m_input_pane{};
    olagarro::oui::TextEdit* m_input{};
    bool m_applies{true};
    ErrorLine* m_error_line_view{};
    std::optional<std::string> m_result;
    std::optional<std::size_t> m_error_line;
    olagarro::signal_slot::Signal<> m_apply;
    olagarro::signal_slot::Signal<std::size_t> m_error_chosen;
};
