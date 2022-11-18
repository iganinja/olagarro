#ifndef EXECUTE_H
#define EXECUTE_H

#include "tasks/task.h"

namespace Olagarro
{
namespace Tasks
{

template<typename Callable>
class ExecuteCallable : public Task
{
public:
    ExecuteCallable(Callable callable) :
        Task{"ExecuteCallable"},
        mCallable{callable}
    {
    }

    State taskUpdate(float deltaTime) override
    {
        mCallable();
        return State::Finished;
    }

private:
    Callable mCallable;
};

template<typename Callable>
TaskUP execute(Callable callable)
{
    return std::make_unique<ExecuteCallable<Callable>>(callable);
}

}
}

#endif // EXECUTE_H
