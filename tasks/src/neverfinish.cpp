#include "tasks/neverfinish.h"

namespace olagarro
{
namespace tasks
{

namespace
{

class NeverFinish : public Task
{
  public:
    explicit NeverFinish(TaskPtr&& task) : Task{"NeverFinish"}, m_task{std::move(task)} {}

    std::vector<const Task*> children() const override
    {
        if(not m_task)
        {
            return {};
        }

        return {m_task.get()};
    }

    std::vector<std::unique_ptr<Task>> grab_children() override
    {
        std::vector<std::unique_ptr<Task>> children;

        if(m_task)
        {
            children.emplace_back(std::move(m_task));
        }

        return children;
    }

  protected:
    void task_on_about_to_start() override
    {
        m_task_finished = not m_task;

        if(m_task)
        {
            m_task->on_about_to_start();
        }
    }

    State task_update(float delta_time) override
    {
        if(not m_task_finished and m_task->update(delta_time) == State::finished)
        {
            m_task_finished = true;
        }

        return State::working;
    }

  private:
    TaskPtr m_task;
    bool m_task_finished{false};
};

} // namespace

TaskPtr never_finish(TaskPtr&& task)
{
    return std::make_unique<NeverFinish>(std::move(task));
}

} // namespace tasks
} // namespace olagarro
