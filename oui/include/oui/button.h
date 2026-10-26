#pragma once

#include <oui/theme.h>

#include <oui/container.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

class Button final : public Widget
{
  public:
    explicit Button(std::string_view text);

    void set_text(std::string_view text);
    void set_shadow_visible(bool visible);
    // What lies around the button, where its shadow falls; the theme's dialog background when not set.
    void set_background(ColorRef background);
    void set_compact(bool compact);
    // Makes it the button Enter presses from anywhere in its window, unless what has the focus takes Enter itself; it
    // shows apart, as Turbo Vision's default button.
    void set_default(bool is_default);
    [[nodiscard]] bool is_default() const;
    // Shown in place of the text where the painter draws icons, as a graphical style does; a terminal still writes the
    // text, which also stays its name for tests and descriptions.
    void set_icon(std::optional<Icon> icon);
    [[nodiscard]] std::optional<Icon> icon() const;

    [[nodiscard]] std::string_view text() const;
    // The letter that presses it with Alt in a dialog, marked in its text with &, as a menu's: "&Open". && is an &.
    [[nodiscard]] std::optional<char32_t> mnemonic() const;

    template <typename Callback> olagarro::signal_slot::Connection on_clicked(Callback&& callback)
    {
        return m_clicked.connect(std::forward<Callback>(callback));
    }

    void click();

    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    std::string m_text;
    bool m_pressed{};
    bool m_shadow_visible{true};
    std::optional<ColorRef> m_background;
    bool m_compact{false};
    bool m_default{false};
    std::optional<Icon> m_icon;
    olagarro::signal_slot::Signal<> m_clicked;
};

[[nodiscard]] std::unique_ptr<Button> button(std::string_view text);
Button& button(std::string_view text, Container& parent);

template <typename Callback> Button& button(std::string_view text, Container& parent, Callback&& on_clicked)
{
    auto& result{button(text, parent)};
    result.on_clicked(std::forward<Callback>(on_clicked));
    return result;
}

} // namespace olagarro::oui
