#include <tasks/execute.h>
#include <tasks/serial.h>
#include <tasks/taskexecutor.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::tasks;

TEST(TaskExecutorTests, DestroysDeepTaskTreesIteratively)
{
    TaskPtr task{execute([] {})};

    constexpr std::size_t depth{20000};
    for(std::size_t index{0}; index < depth; ++ index)
    {
        task = serial(std::move(task));
    }

    TaskExecutor executor;
    executor.add_task(std::move(task));
}

// A task that adds another one from inside its own update used to push into the vector the executor was walking, and
// the reallocation left the walk on freed memory. The added task waits for the next update.
TEST(TaskExecutorTests, ATaskAddedDuringAnUpdateRunsOnTheNextOne)
{
    TaskExecutor executor;
    int added_runs{0};

    executor.add_task(execute(
        [&]
        {
            for(int index{0}; index < 64; ++ index)
            {
                executor.add_task(execute([&] { ++ added_runs; }));
            }
        }));

    executor.update(1.0f);
    EXPECT_EQ(added_runs, 0);

    executor.update(1.0f);
    EXPECT_EQ(added_runs, 64);
}

TEST(TaskExecutorTests, IsEmptyCountsTheTasksWaitingForTheNextUpdate)
{
    TaskExecutor executor;
    EXPECT_TRUE(executor.is_empty());

    executor.add_task(execute([&] { executor.add_task(execute([] {})); }));
    EXPECT_FALSE(executor.is_empty());

    executor.update(1.0f);
    EXPECT_FALSE(executor.is_empty());

    executor.update(1.0f);
    EXPECT_TRUE(executor.is_empty());
}

TEST(TaskExecutorTests, CurrentDeltaIsTheDeltaOfTheUpdateInProgress)
{
    TaskExecutor executor;
    EXPECT_EQ(executor.current_delta(), 0.0f);

    float seen{0.0f};
    executor.add_task(execute([&] { seen = executor.current_delta(); }));

    executor.update(0.125f);
    EXPECT_EQ(seen, 0.125f);
    EXPECT_EQ(executor.current_delta(), 0.125f);
}

// clear() from a task's update destroyed the tasks the update was going through.
TEST(TaskExecutorTests, ClearingFromATaskEndsTheUpdateThereAndEmptiesIt)
{
    TaskExecutor executor;
    int later_runs{0};
    executor.add_task(execute([&] { executor.clear(); }));
    executor.add_task(execute([&] { ++ later_runs; }));
    executor.update(1.0f);
    EXPECT_TRUE(executor.is_empty());
    EXPECT_EQ(later_runs, 0);
    executor.update(1.0f);
    EXPECT_EQ(later_runs, 0);
}
