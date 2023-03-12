#pragma once

#include <algorithm>
#include <tuple>
#include <chrono>

#include "parallel.h"


namespace Olagarro
{
namespace CTTasks
{

using clock = std::chrono::high_resolution_clock;

template<typename... Tasks>
class TaskExecutor
{
public:
    TaskExecutor(Tasks... tasks) :
        mTasks{tasks...}
    {
        mTasks.onAboutToStart();
        mLastTimePoint = clock::now();
    }

    void update()
    {
        const auto now{clock::now()};
        const auto deltaTime{std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastTimePoint).count() / 1000.0f};

        mTasks.update(deltaTime);

        mLastTimePoint = now;
    }

private:
    Parallel<Tasks...> mTasks;
    std::chrono::time_point<clock> mLastTimePoint;
};

}
}
