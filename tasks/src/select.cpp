#include "tasks/select.h"

namespace olagarro
{
namespace tasks
{

class Select : public Task
{
  public:
    Select(std::vector<SelectEntry>&& entries) : Task{"Select"}
    {
        for(auto& entry : entries)
        {
            // An entry with no task, or no predicate to choose it, is left out, as containers leave null tasks.
            if(not entry.task or not entry.predicate)
            {
                continue;
            }

            m_entries.push_back(std::move(entry));
        }
    }

    void task_on_about_to_start() override
    {
        m_current_task = nullptr;

        for(auto& entry : m_entries)
        {
            entry.predicate->on_about_to_start();
        }
    }

    std::vector<const Task*> children() const override
    {
        std::vector<const Task*> children;
        for(const auto& entry : m_entries)
        {
            children.push_back(entry.predicate.get());
            children.push_back(entry.task.get());
        }
        return children;
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children;
        children.reserve(m_entries.size() * 2);

        for(auto& entry : m_entries)
        {
            children.emplace_back(std::move(entry.predicate));
            children.emplace_back(std::move(entry.task));
        }

        m_entries.clear();
        m_current_task = nullptr;
        return children;
    }

    State task_update(float delta_time) override
    {
        if(m_current_task)
        {
            return m_current_task->update(delta_time);
        }

        // Every predicate is updated, but only the first one that holds picks the task.
        for(auto& entry : m_entries)
        {
            if(entry.predicate->update(delta_time) == State::finished and not m_current_task)
            {
                m_current_task = entry.task.get();
                m_current_task->on_about_to_start();
            }
        }

        return State::working;
    }

  private:
    std::vector<SelectEntry> m_entries;
    Task* m_current_task{nullptr};
};

namespace Implementation
{
TaskPtr make_select(std::vector<SelectEntry>&& entries)
{
    return std::make_unique<Select>(std::move(entries));
}
} // namespace Implementation

} // namespace tasks
} // namespace olagarro
