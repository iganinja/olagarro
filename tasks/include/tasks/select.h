#ifndef SELECT_H
#define SELECT_H

#include "tasks/task.h"

#include <utility>

namespace olagarro
{
namespace tasks
{

struct SelectEntry
{
    SelectEntry(TaskPtr&& predicate, TaskPtr&& task) : predicate{std::move(predicate)}, task{std::move(task)} {}

    TaskPtr predicate;
    TaskPtr task;
};

namespace Implementation
{
TaskPtr make_select(std::vector<SelectEntry>&& entries);
}

template <typename... SelectEntries> TaskPtr select(SelectEntries&&... select_entries)
{
    SelectEntry entries[] = {std::forward<SelectEntries>(select_entries)...};
    return Implementation::make_select(std::vector<SelectEntry>{std::make_move_iterator(std::begin(entries)),
                                                                std::make_move_iterator(std::end(entries))});
}

} // namespace tasks
} // namespace olagarro

#endif // SELECT_H
