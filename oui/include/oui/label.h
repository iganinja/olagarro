#pragma once

#include <oui/painter.h>
#include <oui/theme.h>

#include <oui/container.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{

class Label final : public Widget
{
  public:
    explicit Label(std::string_view text);
    void set_text(std::string_view text);
    void set_wrap(bool enabled);
    void set_centered(bool enabled);
    void set_right_aligned(bool enabled);
    void set_color(ColorRef color);
    // The colour behind the text; the theme's dialog background when not set.
    void set_background(ColorRef background);
    void set_bold(bool enabled);
    // A mark before the text in a column of `columns` cells, as a folder's in a file list; see Marker.
    void set_marker(std::string text, int columns, std::optional<Icon> icon = std::nullopt);
    // The mark as the painter takes it, if the label has one.
    [[nodiscard]] std::optional<Marker> marker() const;
    [[nodiscard]] bool wraps() const;
    [[nodiscard]] bool is_centered() const;
    [[nodiscard]] bool is_right_aligned() const;
    [[nodiscard]] bool is_bold() const;
    // The colour set for the text, if one was.
    [[nodiscard]] std::optional<ColorRef> color() const;
    [[nodiscard]] std::string_view text() const;
    // Makes it name `buddy`, as "&Name:" names the field after it: its text then marks with & the letter that, with Alt
    // in a dialog, gives `buddy` the focus, shown apart as Turbo Vision's labels show it. && is an &.
    void set_buddy(Widget& buddy);
    // What it names, while it is there.
    [[nodiscard]] Widget* buddy() const;
    // The letter that goes to what it names; none without a buddy.
    [[nodiscard]] std::optional<char32_t> mnemonic() const;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    std::optional<ColorRef> m_background;
    std::string m_text;
    std::optional<ColorRef> m_color;
    bool m_wrap{false};
    bool m_centered{false};
    bool m_right_aligned{false};
    bool m_bold{false};
    std::string m_marker_text;
    int m_marker_columns{0};
    std::optional<Icon> m_marker_icon;
    bool m_has_marker{false};
    WidgetRef<Widget> m_buddy;
};

[[nodiscard]] std::unique_ptr<Label> label(std::string_view text);
Label& label(std::string_view text, Container& parent);

} // namespace olagarro::oui
