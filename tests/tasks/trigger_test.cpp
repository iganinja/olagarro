#include <tasks/execute.h>
#include <tasks/repeat.h>
#include <tasks/serial.h>
#include <tasks/taskexecutor.h>
#include <tasks/trigger.h>

#include <gtest/gtest.h>

using namespace olagarro;
using namespace olagarro::tasks;

namespace
{

TEST(TriggerTests, FiringItFinishesTheTask)
{
    auto [task, trigger] = make_trigger();
    bool after{false};

    TaskExecutor executor;
    executor.add_task(serial(std::move(task), execute([&] { after = true; })));

    executor.update(1.0f);
    executor.update(1.0f);
    EXPECT_FALSE(after);

    trigger.fire();
    executor.update(1.0f);
    EXPECT_TRUE(after);
    EXPECT_TRUE(executor.is_empty());
}

TEST(TriggerTests, FiringAfterTheTaskIsGoneDoesNothing)
{
    auto [task, trigger] = make_trigger();

    {
        TaskExecutor executor;
        executor.add_task(std::move(task));
        executor.update(1.0f);
    }

    trigger.fire();
    const auto copy{trigger};
    copy.fire();
}

TEST(TriggerTests, StartingItAgainForgetsTheEarlierFire)
{
    auto [task, trigger] = make_trigger();
    int finished{0};

    TaskExecutor executor;
    executor.add_task(repeat_forever(serial(std::move(task), execute([&] { ++ finished; }))));

    executor.update(1.0f);
    trigger.fire();
    executor.update(1.0f);
    EXPECT_EQ(finished, 1);

    executor.update(1.0f);
    executor.update(1.0f);
    EXPECT_EQ(finished, 1);

    trigger.fire();
    executor.update(1.0f);
    EXPECT_EQ(finished, 2);
}

TEST(TriggerTests, AFireBeforeTheTaskStartsIsForgotten)
{
    auto [task, trigger] = make_trigger();

    TaskExecutor executor;
    executor.add_task(std::move(task));

    trigger.fire();
    executor.update(1.0f);
    EXPECT_FALSE(executor.is_empty());
}

} // namespace
