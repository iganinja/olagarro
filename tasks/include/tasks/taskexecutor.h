#ifndef TASKEXECUTOR_H
#define TASKEXECUTOR_H

#include "task.h"

#include <chrono>
#include <memory>
#include <vector>

namespace olagarro
{
namespace tasks
{

class TaskExecutor
{
  public:
    ~TaskExecutor();

    void add_task(TaskPtr&& task);

    void update(float delta_time);

    // Destroys every task. Called from a task's update, it ends that update when the task returns, the tasks it has
    // not reached left out of it; tasks added after it, in that update, run in the next.
    void clear();

    // Whether it holds no task, counting those added during an update that wait for the next one.
    bool is_empty() const;

    // The delta of the update in progress; between updates, the last one's, and 0 before the first.
    float current_delta() const;

  private:
    std::vector<TaskPtr> m_tasks;
    // Tasks added while update() walks m_tasks wait here until the next update, so the walk never sees its vector
    // reallocate under it.
    std::vector<TaskPtr> m_pending_tasks;
    bool m_updating{false};
    bool m_clear_requested{false};
    float m_current_delta{0.0f};
};

} // namespace tasks
} // namespace olagarro

#endif // TASKEXECUTOR_H
