#pragma once

#include <oui/theme.h>

#include <oui/widget.h>
#include <oui/container.h>
#include <signalslot/signalslot.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

class CheckButton final : public Widget
{
  public:
    explicit CheckButton(std::string_view text);

    void set_text(std::string_view text);
    void set_background(ColorRef background);
    [[nodiscard]] std::string_view text() const;
    void set_checked(bool checked);
    [[nodiscard]] bool is_checked() const;
    // Turns it on or off, as a press does, and says so.
    void toggle();
    // The letter that turns it with Alt in a dialog, marked in its text with &, as a button's. && is an &.
    [[nodiscard]] std::optional<char32_t> mnemonic() const;

    template <typename Callback> olagarro::signal_slot::Connection on_toggled(Callback&& callback)
    {
        return m_toggled.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    std::string m_text;
    bool m_checked{};
    bool m_pressed{};
    std::optional<ColorRef> m_background;
    olagarro::signal_slot::Signal<bool> m_toggled;
};

[[nodiscard]] std::unique_ptr<CheckButton> check_button(std::string_view text);
CheckButton& check_button(std::string_view text, Container& parent);

template <typename Callback> CheckButton& check_button(std::string_view text, Container& parent, Callback&& on_toggled)
{
    auto& result{check_button(text, parent)};
    result.on_toggled(std::forward<Callback>(on_toggled));
    return result;
}

} // namespace olagarro::oui
