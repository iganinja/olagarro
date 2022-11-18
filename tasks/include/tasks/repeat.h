#ifndef REPEAT_H
#define REPEAT_H

#include "tasks/task.h"

namespace Olagarro
{
namespace Tasks
{

template<typename Predicate>
class RepeatWhile : public Task
{
public:
    RepeatWhile(Predicate predicate, TaskUP&& task) :
        mPredicate{predicate}, mTask{std::move(task)}
    {
    }

    std::vector<const Task*> children() const override
    {
        return {mTask.get()};
    }

    std::vector<std::unique_ptr<Task>> grabChildren() override
    {
        std::vector<std::unique_ptr<Task>> children;

        children.emplace_back(std::move(mTask));

        return children;
    }

protected:
    void taskOnAboutToStart() override
    {
        mTask->onAboutToStart();

        mShouldRestartTask = false;
    }

    State taskUpdate(float deltaTime) override
    {
        if(not mPredicate())
        {
            return State::Finished;
        }
        else if(mShouldRestartTask)
        {
            mTask->onAboutToStart();
        }

        const auto result{mTask->update(deltaTime)};

        if(result == State::Finished)
        {
            mShouldRestartTask = true;
        }

        return State::Working;
    }

private:
    Predicate mPredicate;
    TaskUP mTask;
    bool mShouldRestartTask;
};

TaskUP repeat(TaskUP&& task, size_t times);

TaskUP repeatForever(TaskUP&& task);

template<typename Predicate>
TaskUP repeatWhile(Predicate predicate, TaskUP&& task)
{
    return std::make_unique<RepeatWhile<Predicate>>(predicate, std::move(task));
}

}
}


#endif // REPEAT_H
