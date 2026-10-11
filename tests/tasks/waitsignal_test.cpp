#include <tasks/taskexecutor.h>
#include <tasks/waitsignal.h>

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;
using namespace olagarro::tasks;

namespace
{

TEST(WaitSignalTests, FinishesAfterTheSignalFiresAndNotBefore)
{
    signal_slot::Signal<> signal;

    TaskExecutor executor;
    executor.add_task(wait_signal(signal));

    executor.update(1.0f);
    executor.update(1.0f);
    EXPECT_FALSE(executor.is_empty());

    signal.fire();
    EXPECT_FALSE(executor.is_empty());

    executor.update(1.0f);
    EXPECT_TRUE(executor.is_empty());
}

TEST(WaitSignalTests, KeepsWhatTheSignalCarriedAfterTheTaskIsGone)
{
    signal_slot::Signal<int, const std::string&> signal;
    auto task{wait_signal(signal)};
    const auto arguments{task->arguments()};

    TaskExecutor executor;
    executor.add_task(std::move(task));
    executor.update(1.0f);
    EXPECT_FALSE(arguments.received());

    signal.fire(7, "seven");
    signal.fire(8, "eight");
    executor.update(1.0f);
    EXPECT_TRUE(executor.is_empty());

    signal.fire(9, "nine");
    ASSERT_TRUE(arguments.received());
    EXPECT_EQ(std::get<0>(arguments.get()), 7);
    EXPECT_EQ(std::get<1>(arguments.get()), "seven");
}

TEST(WaitSignalTests, DestroyingTheTaskBeforeTheSignalFiresLeavesNoConnection)
{
    signal_slot::Signal<int> signal;
    auto task{wait_signal(signal)};
    const auto arguments{task->arguments()};

    TaskExecutor executor;
    executor.add_task(std::move(task));
    executor.update(1.0f);
    executor.clear();

    // A connection left behind would still write into the arguments the handle shares.
    signal.fire(3);
    EXPECT_FALSE(arguments.received());
}

TEST(WaitSignalTests, AFireBeforeTheTaskStartsDoesNotCount)
{
    signal_slot::Signal<int> signal;
    auto task{wait_signal(signal)};
    const auto arguments{task->arguments()};

    signal.fire(1);

    TaskExecutor executor;
    executor.add_task(std::move(task));
    executor.update(1.0f);
    EXPECT_FALSE(arguments.received());
    EXPECT_FALSE(executor.is_empty());
}

} // namespace
