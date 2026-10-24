#pragma once

#include <oui/container.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace olagarro::oui
{

class LineEdit;

// A number to type, with arrows after it that step it down and up: Up and Down step it too, Page Up and Page Down ten
// steps, and the wheel over it. A number typed is its value at once, kept in range and on a step; what is not a
// number changes nothing. While it is not being typed in, it shows its value as it was kept.
class NumberField final : public Container
{
  public:
    NumberField();

    void set_range(double minimum, double maximum);
    // What the arrows, the keys and the wheel add or take away, and what every value is a whole number of, counted
    // from the minimum: 1 unless set; 0 for any value.
    void set_step(double step);
    // How many digits it shows after the point: none unless set.
    void set_decimals(int decimals);
    void set_value(double value);
    [[nodiscard]] double value() const;
    [[nodiscard]] LineEdit& text_field();
    // Shows the value as it was kept, as Enter does: what is typed is taken already, or left when it is no number.
    void commit();
    // Whether a left press on the number and a move along it change the value, as in Blender: a step for each cell
    // moved right, back for each to the left, a tenth of one while Shift is held (the one way a value falls between
    // steps) and ten while Ctrl is. A click without moving types in it, as it does when not draggable; a double click
    // selects the whole number to type another. Off unless set: a drag then selects the text.
    void set_draggable(bool draggable);

    // Called with the new value when it changes.
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }
    // Called when a drag starts changing the value, at its first move, and when it is let go: what changes on the
    // way is told as on_changed, so a program can make the whole drag one undo step.
    template <typename Callback> olagarro::signal_slot::Connection on_drag_started(Callback&& callback)
    {
        return m_drag_started.connect(std::forward<Callback>(callback));
    }
    template <typename Callback> olagarro::signal_slot::Connection on_drag_finished(Callback&& callback)
    {
        return m_drag_finished.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;

  private:
    // A drag along the number, from its press.
    struct Drag
    {
        MouseEvent press;
        int last_x{};
        // The value dragged to, in range but not yet on a step.
        double value{};
        bool moved{false};
        // Shift was held on the way: the value goes in tenths of a step.
        bool fine{false};
    };

    // `value` in range and on a whole number of `step` from the minimum, any value for a step of 0.
    [[nodiscard]] double fitted(double value, double step) const;
    // Makes `value`, kept in range and on a step, the value, telling of it when it changed; the text stays.
    void take(double value);
    // Makes `value` the value as it is, telling of it when it changed.
    void keep(double value);
    [[nodiscard]] static std::optional<double> parse(std::string_view typed);
    void step_by(double steps);
    void show_value() const;
    // What the pointer does on the field while it is draggable; false for what the field does itself.
    bool drag_with(const MouseEvent& mouse);

    LineEdit* m_field{};
    bool m_draggable{false};
    std::optional<Drag> m_drag;
    // While the value is being shown in the field.
    mutable bool m_showing{false};
    olagarro::signal_slot::Signal<> m_drag_started;
    olagarro::signal_slot::Signal<> m_drag_finished;
    // Where the arrows were drawn, in pixels, when one is over the other; none when side by side.
    mutable std::optional<Rect> m_stacked_arrows;
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_step{1.0};
    int m_decimals{0};
    double m_value{0.0};
    olagarro::signal_slot::Signal<double> m_changed;
    olagarro::signal_slot::Connection m_typed;
};

[[nodiscard]] std::unique_ptr<NumberField> number_field();
NumberField& number_field(Container& parent);

} // namespace olagarro::oui
