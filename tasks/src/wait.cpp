#include "tasks/wait.h"

namespace olagarro
{
namespace tasks
{

class Wait : public Task
{
  public:
    Wait(float wait_time) : Task{"Wait"}, m_wait_time{wait_time} {}

    void task_on_about_to_start() override { m_waited_time = 0; }

    State task_update(float delta_time) override
    {
        m_waited_time += delta_time;

        return m_waited_time >= m_wait_time ? State::finished : State::working;
    }

  private:
    const float m_wait_time;
    float m_waited_time;
};

namespace
{

class WaitFor : public Task
{
  public:
    WaitFor(std::function<float()> duration) : Task{"WaitFor"}, m_duration{std::move(duration)} {}

  protected:
    void task_on_about_to_start() override
    {
        m_wait_time = m_duration();
        m_waited_time = 0.0f;
    }

    State task_update(float delta_time) override
    {
        m_waited_time += delta_time;

        return m_waited_time >= m_wait_time ? State::finished : State::working;
    }

  private:
    std::function<float()> m_duration;
    float m_wait_time{0.0f};
    float m_waited_time{0.0f};
};

} // namespace

namespace Implementation
{
TaskPtr wait_seconds(float wait_time)
{
    return std::make_unique<::olagarro::tasks::Wait>(wait_time);
}
} // namespace Implementation

TaskPtr wait_for(std::function<float()> duration)
{
    return std::make_unique<WaitFor>(std::move(duration));
}

} // namespace tasks
} // namespace olagarro
