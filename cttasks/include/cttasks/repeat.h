#pragma once

#include <cassert>
#include "taskscommon.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename Task> class Repeat
{
  public:
    Repeat(Task task, std::size_t repeats_number) : m_task{task}, m_repeats_number{repeats_number}
    {
        assert(m_repeats_number > 0 && "Please provide a nonzero repeats number");
    }

    void on_about_to_start()
    {
        m_current_repetitions = 0;
        m_task.on_about_to_start();
    }

    State update(float delta_time)
    {
        if(m_task.update(delta_time) == State::finished)
        {
            ++ m_current_repetitions;

            if(m_current_repetitions >= m_repeats_number)
            {
                return State::finished;
            }

            m_task.on_about_to_start();
        }

        return State::working;
    }

  private:
    Task m_task;
    const std::size_t m_repeats_number;
    std::size_t m_current_repetitions;
};

template <typename Task> Repeat<Task> repeat(Task task, std::size_t repeats_number)
{
    return Repeat<Task>{task, repeats_number};
}

template <typename Task> class RepeatForever
{
  public:
    RepeatForever(Task task) : m_task{task} {}

    void on_about_to_start() { m_task.on_about_to_start(); }

    State update(float delta_time)
    {
        if(m_task.update(delta_time) == State::finished)
        {
            m_task.on_about_to_start();
        }

        return State::working;
    }

  private:
    Task m_task;
};

template <typename Task> RepeatForever<Task> repeat_forever(Task task)
{
    return RepeatForever<Task>{task};
}

template <typename Predicate, typename Task> class RepeatWhile
{
  public:
    RepeatWhile(Predicate predicate, Task task) : m_predicate{predicate}, m_task{task} {}

    void on_about_to_start()
    {
        m_finished = !m_predicate();

        if(!m_finished)
        {
            m_task.on_about_to_start();
        }
    }

    State update(float delta_time)
    {
        if(m_finished)
        {
            return State::finished;
        }

        if(m_task.update(delta_time) == State::finished)
        {
            if(not m_predicate())
            {
                m_finished = true;
                return State::finished;
            }

            m_task.on_about_to_start();
        }

        return State::working;
    }

  private:
    Predicate m_predicate;
    Task m_task;
    bool m_finished{false};
};

template <typename Predicate, typename Task> RepeatWhile<Predicate, Task> repeat_while(Predicate predicate, Task task)
{
    return RepeatWhile<Predicate, Task>{predicate, task};
}

} // namespace ct_tasks
} // namespace olagarro
