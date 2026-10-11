#ifndef REPEAT_H
#define REPEAT_H

#include "tasks/task.h"

namespace olagarro
{
namespace tasks
{

template <typename Predicate> class RepeatWhile : public Task
{
  public:
    // With no task it has nothing to repeat, and finishes on its first update.
    RepeatWhile(Predicate predicate, TaskPtr&& task)
        : Task{"RepeatWhile"}, m_predicate{predicate}, m_task{std::move(task)}
    {
    }

    std::vector<const Task*> children() const override
    {
        return m_task ? std::vector<const Task*>{m_task.get()} : std::vector<const Task*>{};
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children;

        children.emplace_back(std::move(m_task));

        return children;
    }

  protected:
    void task_on_about_to_start() override
    {
        if(not m_task)
        {
            return;
        }
        m_task->on_about_to_start();

        m_should_restart_task = false;
    }

    State task_update(float delta_time) override
    {
        if(not m_task or not m_predicate())
        {
            return State::finished;
        }
        else if(m_should_restart_task)
        {
            m_task->on_about_to_start();
            m_should_restart_task = false;
        }

        const auto result{m_task->update(delta_time)};

        if(result == State::finished)
        {
            m_should_restart_task = true;
        }

        return State::working;
    }

  private:
    Predicate m_predicate;
    TaskPtr m_task;
    bool m_should_restart_task;
};

TaskPtr repeat(TaskPtr&& task, size_t times);

TaskPtr repeat_forever(TaskPtr&& task);

template <typename Predicate> TaskPtr repeat_while(Predicate predicate, TaskPtr&& task)
{
    return std::make_unique<RepeatWhile<Predicate>>(predicate, std::move(task));
}

} // namespace tasks
} // namespace olagarro

#endif // REPEAT_H
