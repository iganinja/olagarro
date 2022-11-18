#include "tasks/repeat.h"

namespace Olagarro
{
namespace Tasks
{

namespace
{

class Counter
{
public:
    Counter(size_t times) : mTimes{times} {}
    bool operator () ()
    {
        if(mTimes == 0)
        {
            return false;
        }

        -- mTimes;

        return true;
    }
private:
    size_t mTimes;
};

}

TaskUP repeat(TaskUP&& task, size_t times)
{
    return repeatWhile(Counter{times}, std::move(task));
}

TaskUP repeatForever(TaskUP&& task)
{
    auto forever = []()
    {
        return true;
    };

    return repeatWhile(forever, std::move(task));
}

}
}
