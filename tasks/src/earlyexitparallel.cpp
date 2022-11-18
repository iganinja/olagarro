#include "tasks/earlyexitparallel.h"

#include <algorithm>

namespace Olagarro
{
namespace Tasks
{

class EarlyExitParallel : public Task
{
public:
    EarlyExitParallel(std::vector<TaskUP>&& tasks) :
        Task{"EarlyExitParallel"},
        mTasks{std::move(tasks)}
    {
        if(mTasks.empty())
        {
            return;
        }

        mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), nullptr), mTasks.end());
    }

    void taskOnAboutToStart() override
    {
        for(auto& task : mTasks)
        {
            task->onAboutToStart();
        }
    }

    Task::State taskUpdate(float deltaTime) override
    {
        if(mTasks.empty())
        {
            return State::Finished;
        }

        for(auto& task : mTasks)
        {
            if(task->update(deltaTime) == State::Finished)
            {
                return State::Finished;
            }
        }

        return State::Working;
    }

private:
    std::vector<TaskUP> mTasks;
};

TaskUP makeEarlyExitParallel(std::vector<TaskUP>&& tasks)
{
    return std::make_unique<EarlyExitParallel>(std::move(tasks));
}

}

}
