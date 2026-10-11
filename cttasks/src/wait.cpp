#include "cttasks/wait.h"

namespace olagarro
{
namespace ct_tasks
{

Wait::Wait(float wait_time) : m_wait_time{wait_time}
{
}

void Wait::on_about_to_start()
{
    m_current_time = 0;
}

State Wait::update(float delta_time)
{
    m_current_time += delta_time;
    return m_current_time >= m_wait_time ? State::finished : State::working;
}

Wait wait(float wait_time)
{
    return Wait(wait_time);
}

} // namespace ct_tasks
} // namespace olagarro
