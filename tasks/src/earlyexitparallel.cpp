#include "tasks/earlyexitparallel.h"

#include <algorithm>

namespace olagarro
{
namespace tasks
{

class EarlyExitParallel : public Task
{
  public:
    EarlyExitParallel(std::vector<TaskPtr>&& tasks, std::vector<TaskPtr>&& sidecars)
        : Task{"EarlyExitParallel"}, m_tasks{std::move(tasks)}
    {
        m_tasks.erase(std::remove(m_tasks.begin(), m_tasks.end(), nullptr), m_tasks.end());

        m_sidecars.reserve(sidecars.size());

        for(auto& sidecar : sidecars)
        {
            if(sidecar)
            {
                m_sidecars.push_back({std::move(sidecar), false});
            }
        }
    }

    void task_on_about_to_start() override
    {
        for(auto& task : m_tasks)
        {
            task->on_about_to_start();
        }

        for(auto& sidecar : m_sidecars)
        {
            sidecar.is_finished = false;
            sidecar.task->on_about_to_start();
        }
    }

    std::vector<const Task*> children() const override
    {
        std::vector<const Task*> children;
        children.reserve(m_tasks.size() + m_sidecars.size());

        for(const auto& task : m_tasks)
        {
            children.push_back(task.get());
        }

        for(const auto& sidecar : m_sidecars)
        {
            children.push_back(sidecar.task.get());
        }

        return children;
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children{std::move(m_tasks)};
        m_tasks.clear();

        for(auto& sidecar : m_sidecars)
        {
            children.emplace_back(std::move(sidecar.task));
        }

        m_sidecars.clear();
        return children;
    }

    Task::State task_update(float delta_time) override
    {
        if(m_tasks.empty())
        {
            return State::finished;
        }

        for(auto& task : m_tasks)
        {
            if(task->update(delta_time) == State::finished)
            {
                return State::finished;
            }
        }

        for(auto& sidecar : m_sidecars)
        {
            if(not sidecar.is_finished and sidecar.task->update(delta_time) == State::finished)
            {
                sidecar.is_finished = true;
            }
        }

        return State::working;
    }

  private:
    struct Sidecar
    {
        TaskPtr task;
        bool is_finished;
    };

    std::vector<TaskPtr> m_tasks;
    std::vector<Sidecar> m_sidecars;
};

TaskPtr make_early_exit_parallel(std::vector<TaskPtr>&& tasks)
{
    return std::make_unique<EarlyExitParallel>(std::move(tasks), std::vector<TaskPtr>{});
}

TaskPtr early_exit_parallel_with_sidecars(std::vector<TaskPtr>&& ends_group, std::vector<TaskPtr>&& sidecars)
{
    return std::make_unique<EarlyExitParallel>(std::move(ends_group), std::move(sidecars));
}

} // namespace tasks

} // namespace olagarro
