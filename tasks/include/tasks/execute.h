#ifndef EXECUTE_H
#define EXECUTE_H

#include "tasks/task.h"

namespace olagarro
{
namespace tasks
{

template <typename Callable> class ExecuteCallable : public Task
{
  public:
    ExecuteCallable(Callable callable) : Task{"ExecuteCallable"}, m_callable{callable} {}

    State task_update(float) override
    {
        m_callable();
        return State::finished;
    }

  private:
    Callable m_callable;
};

template <typename Callable> TaskPtr execute(Callable callable)
{
    return std::make_unique<ExecuteCallable<Callable>>(callable);
}

template <typename Callable> class ExecuteEveryUpdate : public Task
{
  public:
    ExecuteEveryUpdate(Callable callable) : Task{"ExecuteEveryUpdate"}, m_callable{callable} {}

    State task_update(float) override
    {
        m_callable();
        return State::working;
    }

  private:
    Callable m_callable;
};

// Calls the callable on every update, and never finishes.
template <typename Callable> TaskPtr execute_every_update(Callable callable)
{
    return std::make_unique<ExecuteEveryUpdate<Callable>>(callable);
}

template <typename Callable> class ExecuteUntil : public Task
{
  public:
    ExecuteUntil(Callable callable) : Task{"ExecuteUntil"}, m_callable{callable} {}

    State task_update(float delta_time) override { return m_callable(delta_time); }

  private:
    Callable m_callable;
};

// A task made of a callable: it is called on every update with the update's delta, a float, and the Task::State it
// returns is the task's, so it runs until it returns Task::State::finished.
template <typename Callable> TaskPtr execute_until(Callable callable)
{
    return std::make_unique<ExecuteUntil<Callable>>(callable);
}

} // namespace tasks
} // namespace olagarro

#endif // EXECUTE_H
