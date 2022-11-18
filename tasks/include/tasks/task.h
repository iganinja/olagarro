#ifndef TASKS_H
#define TASKS_H

#include <memory>
#include <vector>
#include <string>

#define TASKS_USE_NAMES

namespace Olagarro
{
namespace Tasks
{

class Task
{
public:
    Task(const std::string& name = "Task");

    virtual ~Task() = default;

#ifdef TASKS_USE_NAMES
    const std::string& name() const;

    std::string tree() const;
#endif

    virtual void onAboutToStart();

    virtual bool wasStarted() const;

    enum class State
    {
        Working,
        Finished
    };

    virtual State update(float deltaTime);

    virtual std::vector<const Task*> children() const;

    virtual std::vector<std::unique_ptr<Task>> grabChildren();

protected:
    // Usually a task needs to override those methods
    virtual void taskOnAboutToStart();
    virtual State taskUpdate(float deltaTime) = 0;

#ifdef TASKS_USE_NAMES
    std::string mName;
#endif

private:
    bool mWasStarted = false;
};

using TaskUP = std::unique_ptr<Task>;

}
}

#endif // TASKS_H
