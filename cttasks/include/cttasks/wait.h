#pragma once

#include "taskscommon.h"

#include <type_traits>

namespace olagarro
{
namespace ct_tasks
{

class Wait
{
  public:
    Wait(float wait_time);

    void on_about_to_start();

    State update(float delta_time);

  private:
    const float m_wait_time;
    float m_current_time;
};

Wait wait(float wait_time);

template <typename Predicate> class WaitToPredicate
{
  public:
    WaitToPredicate(Predicate predicate) : m_predicate{predicate} {}

    void on_about_to_start() {}

    State update(float) { return m_predicate() ? State::finished : State::working; }

  private:
    Predicate m_predicate;
};

// A number of seconds, of any arithmetic type, or a predicate that says when the wait is over.
template <typename Predicate> auto wait(Predicate predicate)
{
    if constexpr(std::is_arithmetic_v<Predicate> and not std::is_same_v<Predicate, bool>)
    {
        return Wait{static_cast<float>(predicate)};
    }
    else
    {
        return WaitToPredicate<Predicate>{predicate};
    }
}

} // namespace ct_tasks
} // namespace olagarro
