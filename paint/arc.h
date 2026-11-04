#pragma once

#include <paint/geometry.h>

namespace olagarro::paint
{

// Drawing an arc as with a compass: a click puts its centre, a line from it to the pointer shows the radius until a
// second click sets it and where the arc starts, and then the pointer turns the arc round the centre the way it goes,
// up to the whole circle, until a third click leaves it drawn. Snapping keeps where it starts and how far it turns to
// steps of 15 degrees.
class ArcGesture
{
  public:
    enum class Step
    {
        center,
        radius,
        sweep,
    };

    // An arc as draw_arc takes it.
    struct Arc
    {
        Position center;
        double radius{0.0};
        double start{0.0};
        double sweep{0.0};
    };

    // A click where the pointer is: true when it leaves the arc drawn. A second click on the centre sets no radius.
    bool click(Position at, bool snap);
    // The pointer moved to `at`.
    void move(Position at, bool snap);

    [[nodiscard]] Step step() const { return m_step; }
    [[nodiscard]] Position center() const { return m_arc.center; }
    [[nodiscard]] Position pointer() const { return m_pointer; }
    // The arc as it is, once its radius is set.
    [[nodiscard]] const Arc& arc() const { return m_arc; }

  private:
    Step m_step{Step::center};
    Arc m_arc;
    Position m_pointer;
    // Where the pointer's direction last was, and how far it has turned since the start, unsnapped.
    double m_last_angle{0.0};
    double m_turned{0.0};
};

} // namespace olagarro::paint
