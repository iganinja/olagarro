#include "tasks/task.h"
#include <stack>

namespace Olagarro
{
namespace Tasks
{

namespace
{

std::string nTimesTabs(int times)
{
    std::string result{""};

    for(int i = 0; i < times; ++ i)
    {
        result += "  ";
    }

    return result;
}

}

Task::Task(const std::string& name)
#ifdef TASKS_USE_NAMES
    : mName{name}
#endif
{
}

#ifdef TASKS_USE_NAMES
const std::string& Task::name() const
{
    return mName;
}

std::string Task::tree() const
{
    struct TreeEntry
    {
        const Task* task;
        int level;
    };

    std::stack<TreeEntry> stack;

    stack.push({this, 0});

    std::string resultingTree;

    while(not stack.empty())
    {
        const auto entry = stack.top();
        stack.pop();

        resultingTree += nTimesTabs(entry.level) + entry.task->name();

        for(const auto& child : entry.task->children())
        {
            stack.push({child, entry.level + 1});
        }
    }

    return resultingTree;
}
#endif

void Task::onAboutToStart()
{
    taskOnAboutToStart();
    mWasStarted = true;
}

bool Task::wasStarted() const
{
    return mWasStarted;
}

Task::State Task::update(float deltaTime)
{
    if(taskUpdate(deltaTime) == State::Finished)
    {
        mWasStarted = false;
        return State::Finished;
    }

    return State::Working;
}

std::vector<const Task*> Task::children() const
{
    return {};
}

std::vector<std::unique_ptr<Task>> Task::grabChildren()
{
    return {};
}

void Task::taskOnAboutToStart()
{
}

}
}
