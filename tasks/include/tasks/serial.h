#ifndef SERIAL_H
#define SERIAL_H

#include "task.h"

#include <vector>

namespace Olagarro
{
namespace Tasks
{

TaskUP makeSerial(std::vector<TaskUP>&& tasks);

template<typename... Tasks>
TaskUP serial(Tasks&&... tasks)
{
    TaskUP tasksArray[] = {std::move(tasks)...};
    return makeSerial(std::vector<TaskUP>{std::make_move_iterator(std::begin(tasksArray)), std::make_move_iterator(std::end(tasksArray))});
}

}
}

#endif // SERIAL_H
