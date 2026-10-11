#ifndef NEVERFINISH_H
#define NEVERFINISH_H

#include "tasks/task.h"

namespace olagarro
{
namespace tasks
{

// Runs the task until it finishes, and then keeps working without updating it, so a task that is done does not end an
// early exit parallel or a priority selector's branch. It owns the task.
TaskPtr never_finish(TaskPtr&& task);

} // namespace tasks
} // namespace olagarro

#endif // NEVERFINISH_H
