#ifndef EARLYEXITPARALLEL_H
#define EARLYEXITPARALLEL_H

#include "task.h"

#include <vector>

namespace Olagarro
{
namespace Tasks
{

TaskUP makeEarlyExitParallel(std::vector<TaskUP>&& tasks);

template<typename... Tasks>
TaskUP earlyExitParallel(Tasks&&... tasks)
{
    TaskUP tasksArray[] = {std::move(tasks)...};
    return makeEarlyExitParallel(std::vector<TaskUP>{std::make_move_iterator(std::begin(tasksArray)), std::make_move_iterator(std::end(tasksArray))});
}

}
}


#endif // EARLYEXITPARALLEL_H
