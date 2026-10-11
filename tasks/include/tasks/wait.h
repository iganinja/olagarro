#ifndef WAIT_H
#define WAIT_H

#include "tasks/task.h"

#include <functional>
#include <random>
#include <type_traits>

namespace olagarro
{
namespace tasks
{

template <typename Predicate> class WaitPredicate : public Task
{
  public:
    WaitPredicate(Predicate predicate) : Task{"WaitCallable"}, m_predicate{predicate} {}

    State task_update(float) override { return m_predicate() ? State::finished : State::working; }

  private:
    Predicate m_predicate;
};

namespace Implementation
{
TaskPtr wait_seconds(float wait_time);
}

// A number of seconds, of any arithmetic type, or a predicate that says when the wait is over.
template <typename Predicate> TaskPtr wait(Predicate predicate)
{
    if constexpr(std::is_arithmetic_v<Predicate> and not std::is_same_v<Predicate, bool>)
    {
        return Implementation::wait_seconds(static_cast<float>(predicate));
    }
    else
    {
        return std::make_unique<WaitPredicate<Predicate>>(predicate);
    }
}

template <typename Predicate> class WaitHeld : public Task
{
  public:
    WaitHeld(Predicate predicate, float seconds) : Task{"WaitHeld"}, m_predicate{predicate}, m_seconds{seconds} {}

  protected:
    void task_on_about_to_start() override { m_held_time = 0.0f; }

    State task_update(float delta_time) override
    {
        if(not m_predicate())
        {
            m_held_time = 0.0f;
            return State::working;
        }

        m_held_time += delta_time;

        return m_held_time >= m_seconds ? State::finished : State::working;
    }

  private:
    Predicate m_predicate;
    float m_seconds;
    float m_held_time{0.0f};
};

// Finishes when the predicate has held for the given seconds in a row: an update where it does not hold starts the
// count again. Only the updates where it holds count their time.
template <typename Predicate> TaskPtr wait_held(Predicate predicate, float seconds)
{
    return std::make_unique<WaitHeld<Predicate>>(predicate, seconds);
}

// Waits for as many seconds as the function says when the task starts, asked again on every start.
TaskPtr wait_for(std::function<float()> duration);

// Waits for a time drawn between min_seconds and max_seconds every time the task starts. The generator, a standard
// uniform random bit generator, is not owned: it must outlive the task.
template <typename Generator> TaskPtr wait_random(float min_seconds, float max_seconds, Generator& generator)
{
    return wait_for([min_seconds, max_seconds, &generator]()
                    { return std::uniform_real_distribution<float>{min_seconds, max_seconds}(generator); });
}

} // namespace tasks
} // namespace olagarro

#endif // WAIT_H
