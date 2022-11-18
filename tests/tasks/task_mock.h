#ifndef TASK_MOCK_H
#define TASK_MOCK_H

#include <tasks/task.h>

#include <gmock/gmock.h>

namespace Mocks
{

class TaskMock : public Olagarro::Tasks::Task
{
public:
    MOCK_METHOD(void, onAboutToStart, (), (override));
    MOCK_METHOD(bool, wasStarted, (), (const, override));
    MOCK_METHOD(Olagarro::Tasks::Task::State, update, (float), (override));
    MOCK_METHOD(std::vector<const Olagarro::Tasks::Task*>, children, (), (const, override));
    MOCK_METHOD(std::vector<std::unique_ptr<Olagarro::Tasks::Task>>, grabChildren, (), (override));
    MOCK_METHOD(void, taskOnAboutToStart, (), (override));
    MOCK_METHOD(Olagarro::Tasks::Task::State, taskUpdate, (float), (override));
};

}

#endif // TASK_MOCK_H
