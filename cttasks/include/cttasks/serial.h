#pragma once

#include <tuple>
#include "taskscommon.h"
#include "mputils.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename... Tasks> class Serial
{
  public:
    Serial(Tasks... tasks) : m_tasks{tasks...} {}

    void on_about_to_start()
    {
        m_current_task = 0;
        call_task_on_about_to_start(m_current_task);
    }

    State update(float delta_time)
    {
        while(m_current_task < sizeof...(Tasks))
        {
            State task_state{State::finished};
            const auto update{[delta_time, &task_state](std::size_t, auto& task)
                              { task_state = task.update(delta_time); }};

            visit_tuple_at(m_tasks, m_current_task, update);

            if(task_state == State::working)
            {
                return State::working;
            }

            // The task before had this update's time: the next starts in it, so what follows a wait runs when the wait
            // ends, but with none of it, which would count twice.
            delta_time = 0.0f;
            ++ m_current_task;

            if(m_current_task < sizeof...(Tasks))
            {
                call_task_on_about_to_start(m_current_task);
            }
        }

        return State::finished;
    }

  private:
    void call_task_on_about_to_start(std::size_t index)
    {
        const auto start{[](std::size_t, auto& task) { task.on_about_to_start(); }};

        visit_tuple_at(m_tasks, index, start);
    }

    std::tuple<Tasks...> m_tasks;
    std::size_t m_current_task;
};

template <typename... Tasks> Serial<Tasks...> serial(Tasks... tasks)
{
    return Serial<Tasks...>{tasks...};
}

} // namespace ct_tasks
} // namespace olagarro
