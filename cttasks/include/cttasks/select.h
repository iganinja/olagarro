#pragma once

#include <tuple>
#include <cassert>
#include "mputils.h"
#include "taskscommon.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename PredicateTask, typename Task> class SelectEntry
{
  public:
    SelectEntry(PredicateTask predicate_task, Task task) : m_predicate_task{predicate_task}, m_task{task} {}

    void on_about_to_start() { m_predicate_task.on_about_to_start(); }

    bool check_predicate(float delta_time) { return m_predicate_task.update(delta_time) == State::finished; }

    void on_about_to_start_task() { m_task.on_about_to_start(); }

    State update_task(float delta_time) { return m_task.update(delta_time); }

  private:
    PredicateTask m_predicate_task;
    Task m_task;
};

template <typename PredicateTask, typename Task>
SelectEntry<PredicateTask, Task> select_entry(PredicateTask predicate_task, Task task)
{
    return SelectEntry<PredicateTask, Task>{predicate_task, task};
}

template <typename... Entries> class Select
{
  public:
    Select(Entries... entries) : m_entries{entries...} {}

    void on_about_to_start()
    {
        m_selected_entry = invalid_selection;

        for_each_in_tuple(m_entries, [](std::size_t, auto& entry) { entry.on_about_to_start(); });
    }

    State update(float delta_time)
    {
        if(m_selected_entry != invalid_selection)
        {
            State task_state{State::finished};

            visit_tuple_at(m_entries, m_selected_entry, [delta_time, &task_state](std::size_t, auto& entry)
                           { task_state = entry.update_task(delta_time); });

            return task_state;
        }

        for(auto i{0u}; i < sizeof...(Entries); ++ i)
        {
            visit_tuple_at(m_entries, i,
                           [this, delta_time](std::size_t index, auto& entry)
                           {
                               if(entry.check_predicate(delta_time))
                               {
                                   m_selected_entry = index;
                                   entry.on_about_to_start_task();
                               }
                           });

            if(m_selected_entry != invalid_selection)
            {
                break;
            }
        }

        return State::working;
    }

  private:
    std::tuple<Entries...> m_entries;
    enum : std::size_t
    {
        invalid_selection = static_cast<std::size_t>(-1)
    };
    std::size_t m_selected_entry;
};

template <typename... Entries> Select<Entries...> select(Entries... entries)
{
    return Select<Entries...>{entries...};
}

} // namespace ct_tasks
} // namespace olagarro
