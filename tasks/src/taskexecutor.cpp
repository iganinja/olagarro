#include "tasks/taskexecutor.h"
#include <algorithm>

namespace Olagarro
{
namespace Tasks
{

void TaskExecutor::addTask(TaskUP&& task)
{
    if(not task)
    {
        return;
    }
    mTasks.emplace_back(std::move(task));
}

void TaskExecutor::update(float deltaTime)
{
    auto it = std::remove_if(mTasks.begin(), mTasks.end(), [deltaTime](TaskUP& task)
    {
        if(not task->wasStarted())
        {
            task->onAboutToStart();
        }

        return task->update(deltaTime) == Task::State::Finished;
    });

    mTasks.erase(it, mTasks.end());
}

}
}
