#ifndef TASK_MOCK_H
#define TASK_MOCK_H

#include <tasks/task.h>

#include <gmock/gmock.h>

namespace Mocks
{

class TaskMock : public olagarro::tasks::Task
{
  public:
    MOCK_METHOD(void, on_about_to_start, (), (override));
    MOCK_METHOD(bool, was_started, (), (const, override));
    MOCK_METHOD(olagarro::tasks::Task::State, update, (float), (override));
    MOCK_METHOD(std::vector<const olagarro::tasks::Task*>, children, (), (const, override));
    MOCK_METHOD(std::vector<std::unique_ptr<olagarro::tasks::Task>>, grab_children, (), (override));
    MOCK_METHOD(void, task_on_about_to_start, (), (override));
    MOCK_METHOD(olagarro::tasks::Task::State, task_update, (float), (override));
};

} // namespace Mocks

#endif // TASK_MOCK_H
