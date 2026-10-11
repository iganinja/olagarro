#ifndef EARLYEXITPARALLEL_H
#define EARLYEXITPARALLEL_H

#include "task.h"

#include <utility>
#include <vector>

namespace olagarro
{
namespace tasks
{

TaskPtr make_early_exit_parallel(std::vector<TaskPtr>&& tasks);

// Runs every task of both lists side by side, and finishes the update the first task of ends_group finishes; with
// ends_group empty, at once. A sidecar never ends the group: one that finishes is no longer updated, until the group
// starts again. On the update the group ends, the sidecars are not updated. task_list builds either list.
TaskPtr early_exit_parallel_with_sidecars(std::vector<TaskPtr>&& ends_group, std::vector<TaskPtr>&& sidecars);

template <typename... Tasks> TaskPtr early_exit_parallel(Tasks&&... tasks)
{
    TaskPtr tasks_array[] = {std::forward<Tasks>(tasks)...};
    return make_early_exit_parallel(std::vector<TaskPtr>{std::make_move_iterator(std::begin(tasks_array)),
                                                         std::make_move_iterator(std::end(tasks_array))});
}

} // namespace tasks
} // namespace olagarro

#endif // EARLYEXITPARALLEL_H
