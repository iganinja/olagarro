#pragma once

#include <tuple>
#include <cassert>
#include "mputils.h"
#include "taskscommon.h"

namespace Olagarro
{
namespace CTTasks
{

template<typename PredicateTask, typename Task>
class SelectEntry
{
public:
    SelectEntry(PredicateTask predicateTask, Task task) :
        mPredicateTask{predicateTask},
        mTask{task}
    {
    }

    void onAboutToStart()
    {
        mPredicateTask.onAboutToStart();
    }

    bool checkPredicate(float deltaTime)
    {
        return mPredicateTask.update(deltaTime) == State::Finished;
    }

    void onAboutToStartTask()
    {
        mTask.onAboutToStart();
    }

    State updateTask(float deltaTime)
    {
        return mTask.update(deltaTime);
    }
private:
    PredicateTask mPredicateTask;
    Task mTask;
};

template<typename PredicateTask, typename Task>
SelectEntry<PredicateTask, Task> selectEntry(PredicateTask predicateTask, Task task)
{
    return SelectEntry<PredicateTask, Task>{predicateTask, task};
}

template<typename... Entries>
class Select
{
public:
    Select(Entries... entries) :
        mEntries{entries...}
    {
    }

    void onAboutToStart()
    {
        mSelectedEntry = InvalidSelection;

        forEachInTuple(mEntries, [](std::size_t index, auto& entry)
        {
            entry.onAboutToStart();
        });
    }

    State update(float deltaTime)
    {
        if(mSelectedEntry != InvalidSelection)
        {
            State taskState{State::Finished};

            visitTupleAt(mEntries, mSelectedEntry, [deltaTime, &taskState](std::size_t index, auto& entry)
            {
                taskState = entry.updateTask(deltaTime);
            });

            return taskState;
        }

        for(auto i = 0u; i < sizeof...(Entries); ++ i)
        {
            visitTupleAt(mEntries, i, [this, deltaTime](std::size_t index, auto& entry)
            {
                if(entry.checkPredicate(deltaTime))
                {
                    mSelectedEntry = index;
                    entry.onAboutToStartTask();
                }
            });

            if(mSelectedEntry != InvalidSelection)
            {
                break;
            }
        }

        return State::Working;
    }
private:
    std::tuple<Entries...> mEntries;
    enum : std::size_t
    {
        InvalidSelection = static_cast<std::size_t>(-1)
    };
    std::size_t mSelectedEntry;
};

template<typename... Entries>
class Select<Entries...> select(Entries... entries)
{
    return Select<Entries...>{entries...};
}

}
}
