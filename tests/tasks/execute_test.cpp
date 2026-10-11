#include <tasks/execute.h>
#include <tasks/taskexecutor.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::tasks;

namespace
{

TEST(ExecuteTests, ExecuteEveryUpdateCallsOnEveryUpdateAndNeverFinishes)
{
    int calls{0};

    TaskExecutor executor;
    executor.add_task(execute_every_update([&] { ++ calls; }));

    for(int index{0}; index < 5; ++ index)
    {
        executor.update(1.0f);
    }

    EXPECT_EQ(calls, 5);
    EXPECT_FALSE(executor.is_empty());
}

TEST(ExecuteTests, ExecuteUntilRunsUntilTheCallableSaysFinished)
{
    float elapsed{0.0f};

    TaskExecutor executor;
    executor.add_task(execute_until(
        [&](float delta_time)
        {
            elapsed += delta_time;
            return elapsed >= 1.0f ? Task::State::finished : Task::State::working;
        }));

    executor.update(0.25f);
    executor.update(0.25f);
    EXPECT_FALSE(executor.is_empty());

    executor.update(0.5f);
    EXPECT_TRUE(executor.is_empty());
    EXPECT_EQ(elapsed, 1.0f);
}

} // namespace
