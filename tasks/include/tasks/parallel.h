#ifndef PARALLEL_H
#define PARALLEL_H

#include "task.h"

#include <utility>
#include <vector>

namespace olagarro
{
namespace tasks
{

TaskPtr make_parallel(std::vector<TaskPtr>&& tasks);

template <typename... Tasks> TaskPtr parallel(Tasks&&... tasks)
{
    TaskPtr tasks_array[] = {std::forward<Tasks>(tasks)...};
    return make_parallel(std::vector<TaskPtr>{std::make_move_iterator(std::begin(tasks_array)),
                                              std::make_move_iterator(std::end(tasks_array))});
}

} // namespace tasks
} // namespace olagarro

#endif // PARALLEL_H
