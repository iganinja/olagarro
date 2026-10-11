#ifndef SERIAL_H
#define SERIAL_H

#include "task.h"

#include <utility>
#include <vector>

namespace olagarro
{
namespace tasks
{

TaskPtr make_serial(std::vector<TaskPtr>&& tasks);

template <typename... Tasks> TaskPtr serial(Tasks&&... tasks)
{
    TaskPtr tasks_array[] = {std::forward<Tasks>(tasks)...};
    return make_serial(std::vector<TaskPtr>{std::make_move_iterator(std::begin(tasks_array)),
                                            std::make_move_iterator(std::end(tasks_array))});
}

} // namespace tasks
} // namespace olagarro

#endif // SERIAL_H
