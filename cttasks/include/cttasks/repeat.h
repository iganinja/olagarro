#pragma once

#include <cassert>
#include "taskscommon.h"

namespace Olagarro
{
namespace CTTasks
{

template<typename Task>
class Repeat
{
public:
    Repeat(Task task, std::size_t repeatsNumber) :
        mTask{task},
        mRepeatsNumber{repeatsNumber}
    {
        assert(mRepeatsNumber > 0 && "Please provide a nonzero repeats number");
    }

    void onAboutToStart()
    {
        mCurrentRepetitions = 0;
        mTask.onAboutToStart();
    }

    State update(float deltaTime)
    {
        if(mTask.update(deltaTime) == State::Finished)
        {
            ++ mCurrentRepetitions;

            if(mCurrentRepetitions >= mRepeatsNumber)
            {
                return State::Finished;
            }

            mTask.onAboutToStart();
        }

        return State::Working;
    }

private:
    Task mTask;
    const std::size_t mRepeatsNumber;
    std::size_t mCurrentRepetitions;
};

template<typename Task>
Repeat<Task> repeat(Task task, std::size_t repeatsNumber)
{
    return Repeat<Task>{task, repeatsNumber};
}

template<typename Task>
class RepeatForever
{
public:
    RepeatForever(Task task) :
        mTask{task}
    {
    }

    void onAboutToStart()
    {
        mTask.onAboutToStart();
    }

    State update(float deltaTime)
    {
        if(mTask.update(deltaTime) == State::Finished)
        {
            mTask.onAboutToStart();
        }

        return State::Working;
    }

private:
    Task mTask;
};

template<typename Task>
RepeatForever<Task> repeatForever(Task task)
{
    return RepeatForever<Task>{task};
}

template<typename Predicate, typename Task>
class RepeatWhile
{
public:
    RepeatWhile(Predicate predicate, Task task) :
        mPredicate{predicate},
        mTask{task}
    {
    }

    void onAboutToStart()
    {
        mTask.onAboutToStart();
    }

    State update(float deltaTime)
    {
        if(mTask.update(deltaTime) == State::Finished)
        {
            if(not mPredicate())
            {
                return State::Finished;
            }

            mTask.onAboutToStart();
        }

        return State::Working;
    }

private:
    Predicate mPredicate;
    Task mTask;
};

template<typename Predicate, typename Task>
RepeatWhile<Predicate, Task> repeatWhile(Predicate predicate, Task task)
{
    return RepeatWhile<Predicate, Task>{predicate, task};
}

}
}
