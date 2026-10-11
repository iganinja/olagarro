#pragma once

#include <algorithm>
#include <tuple>
#include "taskscommon.h"
#include "mputils.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename... Tasks> class EarlyExitParallel
{
  public:
    EarlyExitParallel(Tasks... tasks) : m_tasks{tasks...} {}

    void on_about_to_start()
    {
        for_each_in_tuple(m_tasks, [](std::size_t, auto& task) { task.on_about_to_start(); });
    }

    State update(float delta_time)
    {
        State task_state{State::working};

        for_each_in_tuple(m_tasks,
                          [delta_time, &task_state](std::size_t, auto& task)
                          {
                              if(task_state == State::finished)
                              {
                                  return;
                              }

                              if(task.update(delta_time) == State::finished)
                              {
                                  task_state = State::finished;
                                  return;
                              }
                          });

        return task_state;
    }

  private:
    std::tuple<Tasks...> m_tasks;
};

template <typename... Tasks> EarlyExitParallel<Tasks...> early_exit_parallel(Tasks... tasks)
{
    return EarlyExitParallel<Tasks...>{tasks...};
}

} // namespace ct_tasks
} // namespace olagarro
