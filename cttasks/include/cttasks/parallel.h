#pragma once

#include <algorithm>
#include <tuple>
#include "taskscommon.h"
#include "mputils.h"

namespace Olagarro
{
namespace CTTasks
{

template<typename... Tasks>
class Parallel
{
public:
    Parallel(Tasks... tasks) :
        mTasks{tasks...}
    {
    }

    void onAboutToStart()
    {
        std::fill(std::begin(mFinishedTasks), std::end(mFinishedTasks), false);

        forEachInTuple(mTasks, [](std::size_t index, auto& task)
        {
            task.onAboutToStart();
        });
    }

    State update(float deltaTime)
    {
        State taskState{State::Finished};

        forEachInTuple(mTasks, [this, deltaTime, &taskState](std::size_t index, auto& task)
        {
            if(mFinishedTasks[index])
            {
                return;
            }

            if(task.update(deltaTime) == State::Finished)
            {
                mFinishedTasks[index] = true;
                return;
            }

            taskState = State::Working;
        });

        return taskState;
    }
private:
    std::tuple<Tasks...> mTasks;
    bool mFinishedTasks[sizeof...(Tasks)];
};

template<typename... Tasks>
Parallel<Tasks...> parallel(Tasks... tasks)
{
    return Parallel<Tasks...>{tasks...};
}

}
}
