#pragma once

#include <oui/container.h>
#include <oui/expression.h>
#include <oui/label.h>
#include <oui/line_edit.h>
#include <oui/text_edit.h>

#include <signalslot/signalslot.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

class Application;
class AngleSwitch;

// A calculator as a prompt: an expression is written on the line at the bottom, what it comes to shows under the log
// as it is written, and Enter puts both in the log and clears the line for the next. Up and Down bring back what was
// written before. "name = expression" keeps a value for later; deg and rad say how angles are measured, clear empties
// the log, help lists what the calculator knows, and exit asks to close it, none of them a name. A click on the unit
// in the corner switches it too, and F2 opens its options. Calculator says what it understands.
class CalculatorView final : public Container
{
  public:
    // The application shows its options, F2's list, over it.
    explicit CalculatorView(Application& application);

    // The line expressions are written on, which has the focus.
    [[nodiscard]] LineEdit& prompt();
    // Works out what is written, as Enter does.
    void submit();
    [[nodiscard]] std::string_view log() const;
    // What went into the log since it was last cleared, as it was written: to keep, and to bring back with replay,
    // which works each out again, values kept and angles and all.
    [[nodiscard]] const std::vector<std::string>& entries() const;
    void replay(const std::vector<std::string>& entries);
    // When exit is written: for whoever shows the calculator to close it, the program or the tab. The line is clear by
    // then, and the calculator does nothing more, so it may be closed at once.
    template <typename Callback> olagarro::signal_slot::Connection on_exit(Callback&& callback)
    {
        return m_exit.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    bool handle_event(const Event& event) override;

  private:
    // What the line comes to, or why it comes to nothing, under the log.
    void show_preview();
    void add_to_log(std::string_view text);
    // To what was written before, or after, as Up and Down go.
    void recall(int step);
    // Measures angles in degrees or in radians, as deg and rad do, and says so in the log.
    void use_angles(bool degrees);
    // F2: the calculator's options, as boxes to tick, all from the keyboard.
    void show_options();
    void clear_log();

    Application& m_application;
    Calculator m_calculator;
    TextEdit* m_log{};
    Label* m_preview{};
    AngleSwitch* m_angles{};
    LineEdit* m_prompt{};
    std::vector<std::string> m_history;
    // Where Up and Down are in the history; its end is the line being written, kept aside while going back.
    std::size_t m_recalled{};
    std::string m_draft;
    std::vector<std::string> m_entries;
    olagarro::signal_slot::Signal<> m_exit;
};

// A key of the program the calculator is in, and what it does, for the calculator's help to list with its own.
struct CalculatorKey
{
    std::string keys;
    std::string action;
};

// How to use the calculator, as Markdown in the language installed: what it understands, with examples, its
// commands and its keys, and after them those of the program it is in. Its title is a first-level heading.
[[nodiscard]] std::string calculator_help(const std::vector<CalculatorKey>& program_keys = {});
// Shows it in a dialog, as a manual page.
void show_calculator_help(Application& application, const std::vector<CalculatorKey>& program_keys = {});

} // namespace olagarro::oui
