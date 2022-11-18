#ifndef WAIT_H
#define WAIT_H

#include "tasks/task.h"

#include <type_traits>

namespace Olagarro
{
namespace Tasks
{

template<typename Predicate>
class WaitPredicate : public Task
{
public:
    WaitPredicate(Predicate predicate) :
        Task{"WaitCallable"},
        mPredicate{predicate}
    {
    }

    State taskUpdate(float deltaTime) override
    {
        return mPredicate()? State::Finished : State::Working;
    }

private:
    Predicate mPredicate;
};

template<typename Predicate>
TaskUP wait(Predicate predicate)
{
    return std::make_unique<WaitPredicate<Predicate>>(predicate);
}

namespace Implementation
{
TaskUP waitSeconds(float waitTime);
}

template<>
inline TaskUP wait(float waitTime)
{
    return Implementation::waitSeconds(waitTime);
}

}
}

#endif // WAIT_H
