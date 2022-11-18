#ifndef PARALLEL_H
#define PARALLEL_H

#include "task.h"

#include <vector>

namespace Olagarro
{
namespace Tasks
{

TaskUP makeParallel(std::vector<TaskUP>&& tasks);

template<typename... Tasks>
TaskUP parallel(Tasks&&... tasks)
{
    TaskUP tasksArray[] = {std::move(tasks)...};
    return makeParallel(std::vector<TaskUP>{std::make_move_iterator(std::begin(tasksArray)), std::make_move_iterator(std::end(tasksArray))});
}

}
}

#endif // PARALLEL_H
