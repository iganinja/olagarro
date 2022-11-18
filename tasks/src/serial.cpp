#include "tasks/serial.h"

#include <algorithm>

namespace Olagarro
{
namespace Tasks
{

class Serial : public Task
{
public:
    Serial(std::vector<TaskUP>&& actions) :
        Task{"Serial"},
        mTasks{std::move(actions)}
    {
        mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), nullptr), mTasks.end());
    }

    std::vector<std::unique_ptr<Task>> grabChildren() override
    {
        std::vector<std::unique_ptr<Task>> children;

        for(auto&& task : mTasks)
        {
            children.emplace_back(std::move(task));
        }
        mTasks.clear();

        return children;
    }

    void taskOnAboutToStart() override
    {
        mCurrentTaskIndex = 0;

        if(not mTasks.empty())
        {
            mTasks[mCurrentTaskIndex]->onAboutToStart();
        }
    }

    Task::State taskUpdate(float deltaTime) override
    {
        if(mTasks.empty())
        {
            return State::Finished;
        }

        if(mTasks[mCurrentTaskIndex]->update(deltaTime) == State::Working)
        {
            return State::Working;
        }

        ++ mCurrentTaskIndex;

        if(mCurrentTaskIndex == mTasks.size())
        {
            return State::Finished;
        }

        mTasks[mCurrentTaskIndex]->onAboutToStart();

        return State::Working;
    }

private:
    std::vector<TaskUP> mTasks;
    std::size_t mCurrentTaskIndex;
};

TaskUP makeSerial(std::vector<TaskUP>&& tasks)
{
    return std::make_unique<Serial>(std::move(tasks));
}

}
}
