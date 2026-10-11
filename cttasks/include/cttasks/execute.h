#pragma once

#include <tuple>
#include "taskscommon.h"

namespace olagarro
{
namespace ct_tasks
{

template <typename Callable> class Execute
{
  public:
    Execute(Callable callable) : m_callable{callable} {}

    void on_about_to_start() {}

    State update(float)
    {
        m_callable();
        return State::finished;
    }

  private:
    Callable m_callable;
};

template <typename Callable> Execute<Callable> execute(Callable callable)
{
    return Execute<Callable>{callable};
}

} // namespace ct_tasks
} // namespace olagarro
