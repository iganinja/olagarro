#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include "tasks/task.h"

#include <functional>
#include <memory>
#include <utility>

namespace olagarro
{
namespace tasks
{

// Curves map the progress of an interpolation, from 0 to 1, to the value it applies, 0 at the start and 1 at the end.
namespace curves
{

inline float linear(float u)
{
    return u;
}

inline float ease_in(float u)
{
    return u * u;
}

inline float ease_out(float u)
{
    return u * (2.0f - u);
}

inline float ease_in_out(float u)
{
    return u < 0.5f ? 2.0f * u * u : -1.0f + (4.0f - 2.0f * u) * u;
}

inline float smoothstep(float u)
{
    return u * u * (3.0f - 2.0f * u);
}

} // namespace curves

// Applies curve(u) on every update, u being the time gone since the task started over the duration, with this update's
// delta already counted: the first update already moves. The update the time runs out on applies exactly 1, without the
// curve, and finishes; a duration of 0 or less does so on the first update. The duration is asked for on every start.
template <typename Duration, typename Apply, typename Curve> class Interpolate : public Task
{
  public:
    Interpolate(Duration duration, Apply apply, Curve curve)
        : Task{"Interpolate"}, m_duration{std::move(duration)}, m_apply{std::move(apply)}, m_curve{std::move(curve)}
    {
    }

  protected:
    void task_on_about_to_start() override
    {
        m_seconds = m_duration();
        m_elapsed = 0.0f;
    }

    State task_update(float delta_time) override
    {
        m_elapsed += delta_time;

        const float u{m_seconds <= 0.0f ? 1.0f : m_elapsed / m_seconds};

        if(u >= 1.0f)
        {
            m_apply(1.0f);
            return State::finished;
        }

        m_apply(m_curve(u));
        return State::working;
    }

  private:
    Duration m_duration;
    Apply m_apply;
    Curve m_curve;
    float m_seconds{0.0f};
    float m_elapsed{0.0f};
};

namespace Implementation
{
template <typename Duration, typename Apply, typename Curve>
TaskPtr interpolate_with(Duration duration, Apply apply, Curve curve)
{
    return std::make_unique<Interpolate<Duration, Apply, Curve>>(std::move(duration), std::move(apply),
                                                                 std::move(curve));
}
} // namespace Implementation

// Calls apply, a void(float) callable, with the curve's value on every update for the given seconds; see Interpolate.
template <typename Apply, typename Curve = float (*)(float)>
TaskPtr interpolate(float seconds, Apply apply, Curve curve = curves::linear)
{
    return Implementation::interpolate_with([seconds]() { return seconds; }, std::move(apply), std::move(curve));
}

// Sets from + (to - from) * curve(u) through setter, a void(Value) callable, on every update for the given seconds, and
// exactly to at the end. Value is any type with +, - and * float.
template <typename Value, typename Setter, typename Curve = float (*)(float)>
TaskPtr interpolate_value(Value from, Value to, float seconds, Setter setter, Curve curve = curves::linear)
{
    return interpolate(
        seconds,
        [from, to, setter](float u) mutable
        {
            // The last update applies exactly 1, and from + (to - from) * 1 may not be exactly to.
            if(u == 1.0f)
            {
                setter(to);
                return;
            }

            setter(from + (to - from) * u);
        },
        std::move(curve));
}

// The outside end of a progress monitor: how far it is, the curve's value from 0 to 1, and exactly 1 once it is over.
// It shares its state with the task, so it stays valid after the task is gone, keeping the last value.
class Progress
{
  public:
    explicit Progress(std::shared_ptr<const float> value) : m_value{std::move(value)} {}

    float value() const { return *m_value; }

  private:
    std::shared_ptr<const float> m_value;
};

// A task that only measures its own progress for others to read, with the timing of Interpolate. It is back to 0 on
// every start, when the duration is asked for again. auto [task, progress] = progress_monitor(2.0f);
template <typename Curve = float (*)(float)>
std::pair<TaskPtr, Progress> progress_monitor(std::function<float()> duration, Curve curve = curves::linear)
{
    auto value{std::make_shared<float>(0.0f)};

    auto task{Implementation::interpolate_with(
        [value, duration = std::move(duration)]()
        {
            *value = 0.0f;
            return duration();
        },
        [value](float u) { *value = u; }, std::move(curve))};

    return {std::move(task), Progress{value}};
}

template <typename Curve = float (*)(float)>
std::pair<TaskPtr, Progress> progress_monitor(float seconds, Curve curve = curves::linear)
{
    return progress_monitor([seconds]() { return seconds; }, std::move(curve));
}

} // namespace tasks
} // namespace olagarro

#endif // INTERPOLATE_H
