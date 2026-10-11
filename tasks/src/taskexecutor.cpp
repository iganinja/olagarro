#include "tasks/taskexecutor.h"
#include <algorithm>

namespace olagarro
{
namespace tasks
{

TaskExecutor::~TaskExecutor()
{
    clear();
}

void TaskExecutor::add_task(TaskPtr&& task)
{
    if(not task)
    {
        return;
    }
    if(m_updating)
    {
        m_pending_tasks.emplace_back(std::move(task));
        return;
    }

    m_tasks.emplace_back(std::move(task));
}

void TaskExecutor::update(float delta_time)
{
    m_current_delta = delta_time;

    for(auto& pending_task : m_pending_tasks)
    {
        m_tasks.emplace_back(std::move(pending_task));
    }
    m_pending_tasks.clear();

    m_updating = true;

    auto task{m_tasks.begin()};
    while(task != m_tasks.end())
    {
        if(not(*task)->was_started())
        {
            (*task)->on_about_to_start();
        }

        const auto state{(*task)->update(delta_time)};
        // Cleared from inside a task: what the update has not reached is not updated, and all goes now that no
        // task is running.
        if(m_clear_requested)
        {
            m_clear_requested = false;
            m_updating = false;
            clear();
            return;
        }
        if(state == Task::State::working)
        {
            ++ task;
            continue;
        }

        TaskPtr completed_task{std::move(*task)};
        task = m_tasks.erase(task);
        destroy_task_tree(std::move(completed_task));
    }

    m_updating = false;
}

void TaskExecutor::clear()
{
    if(m_updating)
    {
        // The task that asked is running: the update clears the rest once it returns.
        m_clear_requested = true;
        for(auto& pending_task : m_pending_tasks)
        {
            destroy_task_tree(std::move(pending_task));
        }
        m_pending_tasks.clear();
        return;
    }
    for(auto& task : m_tasks)
    {
        destroy_task_tree(std::move(task));
    }

    m_tasks.clear();

    for(auto& pending_task : m_pending_tasks)
    {
        destroy_task_tree(std::move(pending_task));
    }

    m_pending_tasks.clear();
}

bool TaskExecutor::is_empty() const
{
    return m_tasks.empty() and m_pending_tasks.empty();
}

float TaskExecutor::current_delta() const
{
    return m_current_delta;
}

} // namespace tasks
} // namespace olagarro
