#include "tasks/parallel.h"

#include <algorithm>
#include <bitset>

namespace Olagarro
{
namespace Tasks
{

class Parallel : public Task
{
public:
    Parallel(std::vector<TaskUP>&& tasks) :
        Task{"Parallel"}
    {
        mTasks.reserve(tasks.size());

        for(auto&& task : tasks)
        {
            if(not task)
            {
                continue;
            }

            mTasks.push_back({std::move(task), false});
        }
    }

    void taskOnAboutToStart() override
    {
        for(auto& taskEntry : mTasks)
        {
            taskEntry.isFinished = false;
            taskEntry.task->onAboutToStart();
        }
    }

    Task::State taskUpdate(float deltaTime) override
    {
        if(mTasks.empty())
        {
            return State::Finished;
        }

        std::size_t finishedTasksCount{0};

        for(auto& taskEntry : mTasks)
        {
            if(taskEntry.isFinished)
            {
                ++ finishedTasksCount;
                continue;
            }

            if(taskEntry.task->update(deltaTime) == State::Finished)
            {
                taskEntry.isFinished = true;
                ++ finishedTasksCount;
            }
        }

        return finishedTasksCount == mTasks.size()? State::Finished : State::Working;
    }

private:
    struct TaskEntry
    {
        TaskUP task;
        bool isFinished;
    };
    std::vector<TaskEntry> mTasks;
};

TaskUP makeParallel(std::vector<TaskUP>&& tasks)
{
    return std::make_unique<Parallel>(std::move(tasks));
}

}
}
