#include "tasks/repeat.h"

namespace olagarro
{
namespace tasks
{

namespace
{

// Runs its task the times asked, one run after another, however many updates each takes; started again, it counts
// again from the start.
class Repeat : public Task
{
  public:
    // With no task it has nothing to repeat, and finishes on its first update.
    Repeat(TaskPtr&& task, size_t times) : Task{"Repeat"}, m_task{std::move(task)}, m_times{times} {}

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
        m_remaining = m_task ? m_times : 0;
        if(m_task)
        {
            m_task->on_about_to_start();
        }
    }

    State task_update(float delta_time) override
    {
        if(m_remaining == 0)
        {
            return State::finished;
        }
        if(m_task->update(delta_time) == State::working)
        {
            return State::working;
        }
        -- m_remaining;
        if(m_remaining == 0)
        {
            return State::finished;
        }
        m_task->on_about_to_start();
        return State::working;
    }

  private:
    TaskPtr m_task;
    size_t m_times;
    size_t m_remaining{};
};

} // namespace

TaskPtr repeat(TaskPtr&& task, size_t times)
{
    return std::make_unique<Repeat>(std::move(task), times);
}

TaskPtr repeat_forever(TaskPtr&& task)
{
    const auto forever{[]() { return true; }};

    return repeat_while(forever, std::move(task));
}

} // namespace tasks
} // namespace olagarro
