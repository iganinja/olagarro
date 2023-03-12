#pragma once

#include "taskscommon.h"

namespace Olagarro
{
namespace CTTasks
{

class Wait
{
public:
    Wait(float waitTime);

    void onAboutToStart();

    State update(float deltaTime);

private:
    const float mWaitTime;
    float mCurrentTime;
};

Wait wait(float waitTime);

template<typename Predicate>
class WaitToPredicate
{
public:
    WaitToPredicate(Predicate predicate) :
        mPredicate{predicate}
    {
    }

    void onAboutToStart()
    {
    }

    State update(float deltaTime)
    {
        return mPredicate? State::Finished : State::Working;
    }
private:
    Predicate mPredicate;
};

template<typename Predicate>
WaitToPredicate<Predicate> wait(Predicate predicate)
{
    return WaitToPredicate<Predicate>{predicate};
}

}
}
