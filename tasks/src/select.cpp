#include "tasks/select.h"

namespace Olagarro
{
namespace Tasks
{

class Select : public Task
{
public:
    Select(std::vector<SelectEntry>&& entries) :
        Task{"Select"}
    {
        for(auto& entry : entries)
        {
            if(not entry.task)
            {
                continue;
            }

            mEntries.push_back(std::move(entry));
        }
    }

    void taskOnAboutToStart() override
    {
        mCurrentTask = nullptr;

        for(auto& entry : mEntries)
        {
            entry.predicate->onAboutToStart();
        }
    }

    State taskUpdate(float deltaTime) override
    {
        if(mCurrentTask)
        {
            return mCurrentTask->update(deltaTime);
        }

        for(auto& entry : mEntries)
        {
            if(entry.predicate->update(deltaTime) == State::Finished)
            {
                mCurrentTask = entry.task.get();
                mCurrentTask->onAboutToStart();
            }
        }

        return State::Working;
    }

private:
    std::vector<SelectEntry> mEntries;
    Task* mCurrentTask{nullptr};
};

namespace Implementation
{
TaskUP makeSelect(std::vector<SelectEntry>&& entries)
{
    return std::make_unique<Select>(std::move(entries));
}
}

}
}
