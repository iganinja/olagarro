#pragma once

#include <oui/widget.h>

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <utility>

namespace olagarro::oui
{

class Container;

// A value picked along a track by its thumb, as a brush's size: with the arrows, Page Up and Page Down, Home and End,
// the wheel, or the mouse, clicking or dragging. In pixels the thumb follows the pointer pixel by pixel.
class Slider final : public Widget
{
  public:
    // Its ends; the value is kept between them.
    void set_range(double minimum, double maximum);
    [[nodiscard]] double minimum() const;
    [[nodiscard]] double maximum() const;
    // What every value is a whole number of, counted from the minimum: 1 unless set; 0 for any value.
    void set_step(double step);
    [[nodiscard]] double step() const;
    void set_value(double value);
    [[nodiscard]] double value() const;

    // Called with the new value when it changes, by the person or by set_value.
    template <typename Callback> olagarro::signal_slot::Connection on_changed(Callback&& callback)
    {
        return m_changed.connect(std::forward<Callback>(callback));
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override;
    void paint(Painter& painter, const Theme& theme) const override;
    [[nodiscard]] bool is_focusable() const override;
    bool handle_event(const Event& event) override;
    [[nodiscard]] bool takes_raw_pointer() const override;

  private:
    // The value at the pointer, from where it is along the track.
    [[nodiscard]] double value_at(const MouseEvent& mouse) const;
    // The value kept in range and on a step.
    [[nodiscard]] double fitted(double value) const;

    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_step{1.0};
    double m_value{0.0};
    // Where it was last painted in pixels, to follow the pointer by them; none in a terminal.
    mutable std::optional<Rect> m_pixels;
    olagarro::signal_slot::Signal<double> m_changed;
};

[[nodiscard]] std::unique_ptr<Slider> slider();
Slider& slider(Container& parent);

} // namespace olagarro::oui
