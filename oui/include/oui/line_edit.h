#pragma once

#include <oui/theme.h>

#include <oui/container.h>
#include <signalslot/signalslot.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace olagarro::oui
{
class LineEdit final : public Widget
{
  public:
    void set_text(std::string_view text);
    [[nodiscard]] std::string_view text() const;
    [[nodiscard]] std::string selected_text() const;
    void select_all();
    // Selects the bytes [start, end) of the text, the cursor at the end; both are clamped to the text.
    void select_range(std::size_t start, std::size_t end);
    // The selection as [first, last) bytes of the text, whichever end the cursor is at; empty at the cursor.
    [[nodiscard]] std::pair<std::size_t, std::size_t> selection_range() const;
    void copy_selection(std::string& clipboard) const;
    void cut_selection(std::string& clipboard);
    void paste(std::string_view text);
    void set_placeholder(std::string_view placeholder);
    void set_background(ColorRef background);
    // A field for a password: a dot shows for each character, copying and cutting take nothing, and Control with an
    // arrow goes to an end rather than to a word's, so nothing of what is typed shows or leaves it.
    void set_password(bool password);
    [[nodiscard]] bool is_password() const;
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_submitted(Callback&& callback)
    {
        return m_submitted.connect(std::forward<Callback>(callback));
    }
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    void replace_selection(std::string_view replacement);
    [[nodiscard]] bool erase_selection();
    void move_left(bool extend_selection);
    void move_right(bool extend_selection);
    void move_word(bool forward, bool extend_selection);
    [[nodiscard]] std::size_t previous_codepoint(std::size_t index) const;
    // What shows: the text, or a dot for each of its characters in a password field.
    [[nodiscard]] std::string shown_text() const;
    // A byte of the text as the byte of what shows, and back.
    [[nodiscard]] std::size_t shown_offset(std::size_t byte) const;
    [[nodiscard]] std::size_t text_offset(std::size_t shown_byte) const;
    // Moves the start of what shows so the cursor shows, with as much of the text before it as fits.
    void keep_cursor_shown() const;
    std::string m_text;
    std::string m_placeholder;
    std::optional<ColorRef> m_background;
    std::size_t m_cursor{};
    std::size_t m_anchor{};
    bool m_mouse_selecting{false};
    bool m_password{false};
    // The byte the field shows from, when its text is wider than it; painting keeps the cursor in sight.
    mutable std::size_t m_first_shown{};
    olagarro::signal_slot::Signal<std::string_view> m_changed;
    olagarro::signal_slot::Signal<std::string_view> m_submitted;
};
[[nodiscard]] std::unique_ptr<LineEdit> line_edit();
LineEdit& line_edit(Container& parent);
} // namespace olagarro::oui
