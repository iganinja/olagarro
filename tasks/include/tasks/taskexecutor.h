#ifndef TASKEXECUTOR_H
#define TASKEXECUTOR_H

#include "task.h"

#include <chrono>
#include <memory>
#include <vector>

namespace Olagarro
{
namespace Tasks
{

class TaskExecutor
{
public:
    void addTask(TaskUP&& task);

    void update(float deltaTime);

private:
    std::vector<TaskUP> mTasks;
};

}
}

#endif // TASKEXECUTOR_H
