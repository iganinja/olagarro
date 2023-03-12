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
class EarlyExitParallel
{
public:
    EarlyExitParallel(Tasks... tasks) :
        mTasks{tasks...}
    {
    }

    void onAboutToStart()
    {
        forEachInTuple(mTasks, [](std::size_t index, auto& task)
        {
            task.onAboutToStart();
        });
    }

    State update(float deltaTime)
    {
        State taskState{State::Working};

        forEachInTuple(mTasks, [this, deltaTime, &taskState](std::size_t index, auto& task)
        {
            if(task.update(deltaTime) == State::Finished)
            {
                taskState = State::Finished;
                return;
            }
        });

        return taskState;
    }
private:
    std::tuple<Tasks...> mTasks;
};

template<typename... Tasks>
EarlyExitParallel<Tasks...> earlyExitParallel(Tasks... tasks)
{
    return EarlyExitParallel<Tasks...>{tasks...};
}

}
}
