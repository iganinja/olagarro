#pragma once

#include <tuple>
#include "taskscommon.h"
#include "mputils.h"

namespace Olagarro
{
namespace CTTasks
{

template<typename... Tasks>
class Serial
{
public:
    Serial(Tasks... tasks) :
        mTasks{tasks...}
    {
    }

    void onAboutToStart()
    {
        mCurrentTask = 0;
        callTaskOnAboutToStart(mCurrentTask);
    }

    State update(float deltaTime)
    {
        State taskState{State::Finished};

        auto update = [deltaTime, &taskState](std::size_t index, auto& task)
        {
            taskState = task.update(deltaTime);
        };

        visitTupleAt(mTasks, mCurrentTask, update);

        if(taskState == State::Working)
        {
            return State::Working;
        }

        ++ mCurrentTask;

        if(mCurrentTask >= sizeof...(Tasks))
        {
            return State::Finished;
        }

        callTaskOnAboutToStart(mCurrentTask);

        return State::Working;
    }
private:
    void callTaskOnAboutToStart(std::size_t index)
    {
        auto start = [](std::size_t index, auto& task)
        {
            task.onAboutToStart();
        };

        visitTupleAt(mTasks, index, start);
    }

    std::tuple<Tasks...> mTasks;
    std::size_t mCurrentTask;
};

template<typename... Tasks>
Serial<Tasks...> serial(Tasks... tasks)
{
    return Serial<Tasks...>{tasks...};
}

}
}
