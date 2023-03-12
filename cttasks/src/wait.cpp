#include "cttasks/wait.h"

namespace Olagarro
{
namespace CTTasks
{

Wait::Wait(float waitTime) :
    mWaitTime{waitTime}
{}

void Wait::onAboutToStart()
{
    mCurrentTime = 0;
}

State Wait::update(float deltaTime)
{
    mCurrentTime += deltaTime;
    return mCurrentTime >= mWaitTime ? State::Finished : State::Working;
}

Wait wait(float waitTime)
{
    return Wait(waitTime);
}

}
}
