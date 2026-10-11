#include "tasks/priorityselector.h"

#include "tasks/wait.h"

namespace olagarro
{
namespace tasks
{

namespace
{

class PrioritySelector : public Task
{
  public:
    PrioritySelector(TaskPtr&& main_predicate, std::vector<PriorityEntry>&& entries)
        : Task{"PrioritySelector"}, m_main_predicate{std::move(main_predicate)}
    {
        for(auto& entry : entries)
        {
            if(not entry.predicate or not entry.task)
            {
                continue;
            }

            m_entries.push_back(std::move(entry));
        }
    }

    std::vector<const Task*> children() const override
    {
        std::vector<const Task*> children;

        if(m_main_predicate)
        {
            children.push_back(m_main_predicate.get());
        }

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
        children.reserve(m_entries.size() * 2 + 1);

        if(m_main_predicate)
        {
            children.emplace_back(std::move(m_main_predicate));
        }

        for(auto& entry : m_entries)
        {
            children.emplace_back(std::move(entry.predicate));
            children.emplace_back(std::move(entry.task));
        }

        m_entries.clear();
        m_active_entry = no_entry;
        return children;
    }

  protected:
    void task_on_about_to_start() override
    {
        m_active_entry = no_entry;

        if(m_main_predicate)
        {
            m_main_predicate->on_about_to_start();
        }

        for(auto& entry : m_entries)
        {
            entry.predicate->on_about_to_start();
        }
    }

    State task_update(float delta_time) override
    {
        if(m_main_predicate and m_main_predicate->update(delta_time) == State::finished)
        {
            return State::finished;
        }

        for(std::size_t index{0}; index < m_entries.size(); ++ index)
        {
            auto& entry{m_entries[index]};

            if(entry.predicate->update(delta_time) == State::working)
            {
                continue;
            }

            if(index != m_active_entry)
            {
                if(m_active_entry != no_entry)
                {
                    m_entries[m_active_entry].predicate->on_about_to_start();
                }

                m_active_entry = index;
                entry.task->on_about_to_start();
            }
            else if(not entry.task->was_started())
            {
                entry.task->on_about_to_start();
            }

            if(entry.task->update(delta_time) == State::finished)
            {
                entry.predicate->on_about_to_start();
            }

            break;
        }

        return State::working;
    }

  private:
    static constexpr std::size_t no_entry{static_cast<std::size_t>(-1)};

    TaskPtr m_main_predicate;
    std::vector<PriorityEntry> m_entries;
    std::size_t m_active_entry{no_entry};
};

} // namespace

PriorityEntry otherwise(TaskPtr&& task)
{
    return {wait([] { return true; }), std::move(task)};
}

TaskPtr make_priority_selector(TaskPtr&& main_predicate, std::vector<PriorityEntry>&& entries)
{
    return std::make_unique<PrioritySelector>(std::move(main_predicate), std::move(entries));
}

} // namespace tasks
} // namespace olagarro
