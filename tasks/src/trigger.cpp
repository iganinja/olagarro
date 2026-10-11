#include "tasks/trigger.h"

namespace olagarro
{
namespace tasks
{

namespace
{

class TriggerTask : public Task
{
  public:
    explicit TriggerTask(std::shared_ptr<bool> fired) : Task{"Trigger"}, m_fired{std::move(fired)} {}

  protected:
    void task_on_about_to_start() override { *m_fired = false; }

    State task_update(float) override { return *m_fired ? State::finished : State::working; }

  private:
    std::shared_ptr<bool> m_fired;
};

} // namespace

Trigger::Trigger(std::shared_ptr<bool> fired) : m_fired{std::move(fired)}
{
}

void Trigger::fire() const
{
    *m_fired = true;
}

std::pair<TaskPtr, Trigger> make_trigger()
{
    auto fired{std::make_shared<bool>(false)};
    return {std::make_unique<TriggerTask>(fired), Trigger{fired}};
}

} // namespace tasks
} // namespace olagarro
