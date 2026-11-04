#include <paint/arc.h>

#include <algorithm>
#include <cmath>

namespace olagarro::paint
{

namespace
{

constexpr auto pi{3.14159265358979323846};
constexpr auto step_of_snap{pi / 12.0};

[[nodiscard]] double snapped(double angle, bool snap)
{
    return snap ? std::round(angle / step_of_snap) * step_of_snap : angle;
}

} // namespace

bool ArcGesture::click(Position at, bool snap)
{
    m_pointer = at;
    switch(m_step)
    {
    case Step::center:
        m_arc = Arc{at};
        m_step = Step::radius;
        return false;
    case Step::radius:
    {
        const auto radius{std::hypot(at.x - m_arc.center.x, at.y - m_arc.center.y)};
        if(radius < 0.5)
        {
            return false;
        }
        m_arc.radius = radius;
        m_last_angle = std::atan2(at.y - m_arc.center.y, at.x - m_arc.center.x);
        m_arc.start = snapped(m_last_angle, snap);
        m_turned = 0.0;
        m_arc.sweep = 0.0;
        m_step = Step::sweep;
        return false;
    }
    case Step::sweep:
        move(at, snap);
        return true;
    }
    return false;
}

void ArcGesture::move(Position at, bool snap)
{
    m_pointer = at;
    if(m_step == Step::radius && snap)
    {
        // The radius's line in steps of 15 degrees, as long as the pointer is far from the centre.
        const auto angle{snapped(std::atan2(at.y - m_arc.center.y, at.x - m_arc.center.x), true)};
        const auto length{std::hypot(at.x - m_arc.center.x, at.y - m_arc.center.y)};
        m_pointer = {m_arc.center.x + std::cos(angle) * length, m_arc.center.y + std::sin(angle) * length};
    }
    if(m_step != Step::sweep)
    {
        return;
    }
    // How far the pointer's direction turned since the last move, the short way round, added up: the arc follows it
    // whichever way it goes, and no further than the whole circle.
    const auto angle{std::atan2(at.y - m_arc.center.y, at.x - m_arc.center.x)};
    auto turned{angle - m_last_angle};
    turned -= std::round(turned / (2.0 * pi)) * 2.0 * pi;
    m_last_angle = angle;
    m_turned = std::clamp(m_turned + turned, -2.0 * pi, 2.0 * pi);
    m_arc.sweep = snapped(m_turned, snap);
}

} // namespace olagarro::paint
