#include "tasks/serial.h"

#include <algorithm>

namespace olagarro
{
namespace tasks
{

class Serial : public Task
{
  public:
    Serial(std::vector<TaskPtr>&& actions) : Task{"Serial"}, m_tasks{std::move(actions)}
    {
        m_tasks.erase(std::remove(m_tasks.begin(), m_tasks.end(), nullptr), m_tasks.end());
    }

    std::vector<const Task*> children() const override
    {
        std::vector<const Task*> children;
        for(const auto& task : m_tasks)
        {
            children.push_back(task.get());
        }
        return children;
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children;

        for(auto&& task : m_tasks)
        {
            children.emplace_back(std::move(task));
        }
        m_tasks.clear();

        return children;
    }

    void task_on_about_to_start() override
    {
        m_current_task_index = 0;

        if(not m_tasks.empty())
        {
            m_tasks[m_current_task_index]->on_about_to_start();
        }
    }

    Task::State task_update(float delta_time) override
    {
        while(m_current_task_index < m_tasks.size())
        {
            if(m_tasks[m_current_task_index]->update(delta_time) == State::working)
            {
                return State::working;
            }

            // The task before had this update's time: the next starts in it, so what follows a wait runs when the wait
            // ends, but with none of it, which would count twice.
            delta_time = 0.0f;
            ++ m_current_task_index;

            if(m_current_task_index < m_tasks.size())
            {
                m_tasks[m_current_task_index]->on_about_to_start();
            }
        }

        return State::finished;
    }

  private:
    std::vector<TaskPtr> m_tasks;
    std::size_t m_current_task_index;
};

TaskPtr make_serial(std::vector<TaskPtr>&& tasks)
{
    return std::make_unique<Serial>(std::move(tasks));
}

} // namespace tasks
} // namespace olagarro
