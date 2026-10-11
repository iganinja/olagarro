#pragma once

#include <algorithm>
#include <iterator>
#include <tuple>
#include "taskscommon.h"
#include "mputils.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename... Tasks> class Parallel
{
  public:
    Parallel(Tasks... tasks) : m_tasks{tasks...} {}

    void on_about_to_start()
    {
        std::fill(std::begin(m_finished_tasks), std::end(m_finished_tasks), false);

        for_each_in_tuple(m_tasks, [](std::size_t, auto& task) { task.on_about_to_start(); });
    }

    State update(float delta_time)
    {
        State task_state{State::finished};

        for_each_in_tuple(m_tasks,
                          [this, delta_time, &task_state](std::size_t index, auto& task)
                          {
                              if(m_finished_tasks[index])
                              {
                                  return;
                              }

                              if(task.update(delta_time) == State::finished)
                              {
                                  m_finished_tasks[index] = true;
                                  return;
                              }

                              task_state = State::working;
                          });

        return task_state;
    }

  private:
    std::tuple<Tasks...> m_tasks;
    bool m_finished_tasks[sizeof...(Tasks)];
};

template <typename... Tasks> Parallel<Tasks...> parallel(Tasks... tasks)
{
    return Parallel<Tasks...>{tasks...};
}

} // namespace ct_tasks
} // namespace olagarro
