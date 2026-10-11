#include "tasks/parallel.h"

#include <algorithm>
#include <bitset>

namespace olagarro
{
namespace tasks
{

class Parallel : public Task
{
  public:
    Parallel(std::vector<TaskPtr>&& tasks) : Task{"Parallel"}
    {
        m_tasks.reserve(tasks.size());

        for(auto&& task : tasks)
        {
            if(not task)
            {
                continue;
            }

            m_tasks.push_back({std::move(task), false});
        }
    }

    void task_on_about_to_start() override
    {
        for(auto& task_entry : m_tasks)
        {
            task_entry.is_finished = false;
            task_entry.task->on_about_to_start();
        }
    }

    std::vector<const Task*> children() const override
    {
        std::vector<const Task*> children;
        for(const auto& task_entry : m_tasks)
        {
            children.push_back(task_entry.task.get());
        }
        return children;
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children;
        children.reserve(m_tasks.size());

        for(auto& task_entry : m_tasks)
        {
            children.emplace_back(std::move(task_entry.task));
        }

        m_tasks.clear();
        return children;
    }

    Task::State task_update(float delta_time) override
    {
        if(m_tasks.empty())
        {
            return State::finished;
        }

        std::size_t finished_tasks_count{0};

        for(auto& task_entry : m_tasks)
        {
            if(task_entry.is_finished)
            {
                ++ finished_tasks_count;
                continue;
            }

            if(task_entry.task->update(delta_time) == State::finished)
            {
                task_entry.is_finished = true;
                ++ finished_tasks_count;
            }
        }

        return finished_tasks_count == m_tasks.size() ? State::finished : State::working;
    }

  private:
    struct TaskEntry
    {
        TaskPtr task;
        bool is_finished;
    };
    std::vector<TaskEntry> m_tasks;
};

TaskPtr make_parallel(std::vector<TaskPtr>&& tasks)
{
    return std::make_unique<Parallel>(std::move(tasks));
}

} // namespace tasks
} // namespace olagarro
