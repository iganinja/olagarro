#pragma once

#include <oui/container.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <utility>

namespace olagarro::oui
{

class LineEdit;

// A number to type, with arrows after it that step it down and up: Up and Down step it too, Page Up and Page Down ten
// steps, and the wheel over it. What is typed is taken on Enter, kept in range and on a step; what is not a number
// changes nothing. While it is not being typed in, it shows its value.
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

    // Called with the new value when it changes.
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void arrange(const Rect& bounds) override;
    void paint(Painter& painter, const Theme& theme) const override;
    bool handle_event(const Event& event) override;

  private:
    [[nodiscard]] double fitted(double value) const;
    void step_by(double steps);
    // Takes what was typed, or shows the value again when it is not a number.
    void take_typed();
    void show_value() const;

    LineEdit* m_field{};
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_step{1.0};
    int m_decimals{0};
    double m_value{0.0};
    olagarro::signal_slot::Signal<double> m_changed;
    olagarro::signal_slot::Connection m_submitted;
};

[[nodiscard]] std::unique_ptr<NumberField> number_field();
NumberField& number_field(Container& parent);

} // namespace olagarro::oui
