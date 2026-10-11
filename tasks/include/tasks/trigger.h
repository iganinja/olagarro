#ifndef TRIGGER_H
#define TRIGGER_H

#include "tasks/task.h"

#include <memory>
#include <utility>

namespace olagarro
{
namespace tasks
{

// The outside end of a trigger task: fire() makes the task finish on its next update. It shares its state with the
// task, so it can be kept, copied and fired after the task is gone, when firing does nothing.
class Trigger
{
  public:
    void fire() const;

  private:
    friend std::pair<TaskPtr, Trigger> make_trigger();

    explicit Trigger(std::shared_ptr<bool> fired);

    std::shared_ptr<bool> m_fired;
};

// A task that finishes when its Trigger fires. Starting it, as an executor does on its first update, forgets an earlier
// fire, so a fire counts from the start on: auto [task, trigger] = make_trigger();
std::pair<TaskPtr, Trigger> make_trigger();

} // namespace tasks
} // namespace olagarro

#endif // TRIGGER_H
