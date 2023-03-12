#pragma once

#include <tuple>
#include "taskscommon.h"

namespace Olagarro
{
namespace CTTasks
{

template<typename Callable>
class Execute
{
public:
    Execute(Callable callable) :
        mCallable{callable}
    {
    }

    void onAboutToStart()
    {
    }

    State update(float deltaTime)
    {
        mCallable();
        return State::Finished;
    }

private:
    Callable mCallable;
};

template<typename Callable>
Execute<Callable> execute(Callable callable)
{
    return Execute<Callable>{callable};
}

}
}
