#pragma once

#include <oui/application.h>
#include <oui/pty.h>
#include <oui/terminal_screen.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

class Application;

// A terminal in a widget: a program, the user's shell unless told another, running in a terminal of its own,
// whose screen the widget shows and to which it sends what is typed and pasted. While focused it takes every key
// a program in a terminal can want, Ctrl and Alt ones too, but for Ctrl+Page Up and Down, Ctrl+Tab and those with
// Ctrl and Shift together, which stay the application's, as moving between tabs. Shift+Page Up and Down, and
// the wheel, look back at what scrolled off; a program that has the whole screen, as an editor, gets the wheel
// as the arrow keys. The mouse selects: dragged, a double click for a word, a triple click for a line; what is
// selected is copied when the button is let go, and with Ctrl+Shift+C. A right click pastes.
class TerminalView final : public Widget
{
  public:
    // Starts the program; throws std::system_error when it cannot, as Pty does.
    explicit TerminalView(Application& application, PtyOptions options = {});
    ~TerminalView() override;

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool claims_key(const Event& key) const override;

    // Sends `text` as pasted: between the marks that say so, when the program asked for them.
    void paste(std::string_view text);
    // The text selected with the mouse, a line for each line it spans, without the spaces at the end of each;
    // empty when nothing is.
    [[nodiscard]] std::string selected_text() const;
    [[nodiscard]] bool is_running() const;
    // What the program called its window, as a shell names the folder it is in; until one does, the name of the
    // program that has the terminal, as sh or top. Empty when neither is known.
    [[nodiscard]] const std::string& title() const;
    [[nodiscard]] const TerminalScreen& screen() const;

    template <typename Callback> olagarro::signal_slot::Connection on_title_changed(Callback&& callback)
    {
        return m_title_changed.connect(std::forward<Callback>(callback));
    }
    // The program ended, with its exit code; the screen stays as it left it.
    template <typename Callback> olagarro::signal_slot::Connection on_finished(Callback&& callback)
    {
        return m_finished.connect(std::forward<Callback>(callback));
    }

  private:
    // A cell of everything the terminal holds, what scrolled off counted first: line 0 is the oldest.
    struct Place
    {
        int line;
        int column;

        [[nodiscard]] bool operator<(const Place& other) const
        {
            return line < other.line || (line == other.line && column < other.column);
        }
        [[nodiscard]] bool operator==(const Place& other) const { return line == other.line && column == other.column; }
    };

    bool handle_selection(const MouseEvent& mouse);
    [[nodiscard]] Place place_at(int x, int y) const;
    [[nodiscard]] const TerminalCell& cell_at(const Place& place) const;
    [[nodiscard]] bool is_selected(int line, int column) const;
    void select_word(const Place& place);
    void copy_selection();
    void take_output(std::string_view text);
    // The title the program set, or the name of the program in front when it set none, told when it changes.
    void refresh_title();
    void send(std::string_view bytes);
    void scroll_back(int lines);

    Application& m_application;
    TerminalScreen m_screen{80, 24};
    std::unique_ptr<Pty> m_pty;
    // How many lines up into what scrolled off the view is; 0 is the screen as the program has it.
    int m_scrolled_back{0};
    // The selection runs from where the press was to where the pointer is, either way round; none when unset.
    std::optional<Place> m_anchor;
    Place m_head{};
    bool m_selecting{false};
    bool m_dragged{false};
    std::string m_title;
    olagarro::signal_slot::Signal<const std::string&> m_title_changed;
    olagarro::signal_slot::Signal<int> m_finished;
    // Looks at the program in front now and then too: one that starts after the output, or writes nothing, still
    // names the view. Last, so it stops before anything it reads goes.
    Timer m_title_timer;
};

// What a terminal sends a program for a key, as xterm does: the character typed, as UTF-8; a control character
// for Ctrl and a letter; ESC before it for Alt; and an escape sequence for the keys that write nothing, the cursor
// keys their application form (ESC O) when the program asked for that. Empty for what sends nothing.
[[nodiscard]] std::string terminal_keys(const Event& event, bool application_cursor_keys);

} // namespace olagarro::oui
