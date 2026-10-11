#ifndef PRIORITYSELECTOR_H
#define PRIORITYSELECTOR_H

#include "tasks/task.h"

#include <utility>
#include <vector>

namespace olagarro
{
namespace tasks
{

// An entry of a priority selector: its task runs while its predicate, a task, finishes and no entry before it does.
struct PriorityEntry
{
    PriorityEntry(TaskPtr&& predicate, TaskPtr&& task) : predicate{std::move(predicate)}, task{std::move(task)} {}

    TaskPtr predicate;
    TaskPtr task;
};

// An entry whose predicate always holds, for the selector's last place: its task runs when no other entry's does.
PriorityEntry otherwise(TaskPtr&& task);

TaskPtr make_priority_selector(TaskPtr&& main_predicate, std::vector<PriorityEntry>&& entries);

// Picks again on every update, unlike select. The predicates are updated in order, and the first one that finishes
// wins: the ones after it are not updated that update. When the winner is not the entry that was running, that one is
// dropped, its predicate started again and its task left to start afresh when it next wins, and the winner's task is
// started. Then the winner's task is updated; when it finishes, its predicate is started again, and the task starts
// again the next update its entry wins.
//
// It finishes when the main predicate finishes, updated first on every update; with no main predicate (nullptr) it
// never finishes. The selector owns every task it is given.
template <typename... PriorityEntries>
TaskPtr priority_selector(TaskPtr&& main_predicate, PriorityEntries&&... priority_entries)
{
    std::vector<PriorityEntry> entries;
    entries.reserve(sizeof...(priority_entries));
    (entries.emplace_back(std::forward<PriorityEntries>(priority_entries)), ...);
    return make_priority_selector(std::move(main_predicate), std::move(entries));
}

} // namespace tasks
} // namespace olagarro

#endif // PRIORITYSELECTOR_H
