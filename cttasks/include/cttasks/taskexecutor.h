#pragma once

#include <algorithm>
#include <tuple>
#include <chrono>

#include "parallel.h"

namespace olagarro
{
namespace ct_tasks
{

using clock = std::chrono::high_resolution_clock;

template <typename... Tasks> class TaskExecutor
{
  public:
    TaskExecutor(Tasks... tasks) : m_tasks{tasks...}
    {
        m_tasks.on_about_to_start();
        m_last_time_point = clock::now();
    }

    void update()
    {
        const auto now{clock::now()};
        // In seconds, as finely as the clock tells: in whole milliseconds, more than a thousand updates a second
        // would each be 0 seconds long.
        const auto delta_time{std::chrono::duration<float>(now - m_last_time_point).count()};

        update(delta_time);

        m_last_time_point = now;
    }

    void update(float delta_time) { m_tasks.update(delta_time); }

  private:
    Parallel<Tasks...> m_tasks;
    std::chrono::time_point<clock> m_last_time_point;
};

} // namespace ct_tasks
} // namespace olagarro
