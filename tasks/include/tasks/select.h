#ifndef SELECT_H
#define SELECT_H

#include "tasks/task.h"

namespace Olagarro
{
namespace Tasks
{

struct SelectEntry
{
    SelectEntry(TaskUP&& predicate, TaskUP&& task) :
        predicate{std::move(predicate)}, task{std::move(task)}
    {}

    TaskUP predicate;
    TaskUP task;
};

namespace Implementation
{
TaskUP makeSelect(std::vector<SelectEntry>&& entries);
}

template<typename... SelectEntries>
TaskUP select(SelectEntries&&... selectEntries)
{
    SelectEntry entries[] = {std::move(selectEntries)...};
    return Implementation::makeSelect(std::vector<SelectEntry>{std::make_move_iterator(std::begin(entries)), std::make_move_iterator(std::end(entries))});
}

}
}

#endif // SELECT_H
