#pragma once

#include <oui/container.h>
#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace olagarro::oui
{

// A one-line field that shows the chosen option and drops down the whole list when opened with Enter,
// Space or a click. The arrow at its right edge marks it as a dropdown.
class ComboBox final : public Widget
{
  public:
    explicit ComboBox(std::vector<std::string> options = {}, std::size_t selected = 0);

    void set_options(std::vector<std::string> options);
    [[nodiscard]] const std::vector<std::string>& options() const;
    void set_selected(std::size_t index);
    [[nodiscard]] std::size_t selected() const;
    // Fires with the new index whenever the user picks an option.
    template <typename Callback> olagarro::signal_slot::Connection on_selected(Callback&& callback)
    {
        return m_selected_changed.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    // While open, the arrows, Home and End choose among its options before any shortcut of the application.
    [[nodiscard]] bool claims_key(const Event& event) const override;
    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    [[nodiscard]] Rect paint_bounds() const override;
    [[nodiscard]] Widget* hit_test(int x, int y) override;
    [[nodiscard]] bool is_overlay() const override;
    void dismiss_overlays_at(int x, int y) override;
    void paint(Painter& painter, const Theme& theme) const override;

  private:
    void choose(std::size_t index);

    std::vector<std::string> m_options;
    std::size_t m_selected{};
    bool m_expanded{false};
    // The option under the pointer while the list is open, marked as a menu marks its entries.
    std::optional<std::size_t> m_hovered;
    olagarro::signal_slot::Signal<std::size_t> m_selected_changed;
};

[[nodiscard]] std::unique_ptr<ComboBox> combo_box(std::vector<std::string> options = {}, std::size_t selected = 0);
ComboBox& combo_box(std::vector<std::string> options, std::size_t selected, Container& parent);

} // namespace olagarro::oui
