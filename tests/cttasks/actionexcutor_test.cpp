#include <cttasks/taskexecutor.h>
#include <cttasks/serial.h>
#include <cttasks/parallel.h>
#include "cttasks/earlyexitparallel.h"
#include "cttasks/repeat.h"
#include <cttasks/execute.h>
#include <cttasks/wait.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

#include <iostream>

using namespace olagarro;
using namespace olagarro::ct_tasks;
using ::testing::_;
using ::testing::Exactly;
using ::testing::Return;

namespace
{

class TaskExecutorTests : public ::testing::Test
{
    void SetUp() override { srand(time(NULL)); }
};

class ValueTestTask
{
  public:
    ValueTestTask(int& value) : m_value{value} {}

    void on_about_to_start() { ++ m_value; }

    State update(float) { return State::finished; }

  private:
    int& m_value;
};

TEST_F(TaskExecutorTests, ActionExecutorCallsOnAboutToStart)
{
    std::array<int, 3> values{10, 20, 30};

    TaskExecutor TE{ValueTestTask{values[0]}, ValueTestTask{values[1]}, ValueTestTask{values[2]}};

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 21);
    EXPECT_EQ(values[2], 31);
}

// The executor measured the time between updates in whole milliseconds: updated more than a thousand times a second,
// every update was 0 seconds long, and a timed wait never ended.
TEST_F(TaskExecutorTests, TimesUpdatesCloserThanAMillisecond)
{
    auto done{false};
    TaskExecutor executor{serial(wait(0.01f), execute([&done] { done = true; }))};
    const auto until{std::chrono::steady_clock::now() + std::chrono::milliseconds{30}};
    while(not done and std::chrono::steady_clock::now() < until)
    {
        executor.update();
    }
    EXPECT_TRUE(done);
}

// wait took seconds only as a float: wait(1.0) or wait(1) were taken for predicates and did not compile.
TEST_F(TaskExecutorTests, WaitTakesSecondsAsAnyNumber)
{
    auto done{false};
    TaskExecutor executor{serial(wait(1.0), wait(1), execute([&done] { done = true; }))};
    // Half a second at a time: the first wait ends in the second update, the other in the fourth, each a second.
    for(int update{}; update < 3; ++ update)
    {
        executor.update(0.5f);
    }
    EXPECT_FALSE(done);
    executor.update(0.5f);
    EXPECT_TRUE(done);
}

// A serial gave the next task the whole of the update the one before it finished in, so the time counted twice: two
// waits of a second ended together in an update of a second and a half.
TEST_F(TaskExecutorTests, ASerialStartsTheNextTaskWithNoTimeOfItsOwn)
{
    auto done{false};
    TaskExecutor executor{serial(wait(1.0f), wait(1.0f), execute([&done] { done = true; }))};
    executor.update(1.5f);
    EXPECT_FALSE(done);
    executor.update(1.0f);
    EXPECT_TRUE(done) << "and what follows a wait still runs in the update it ends";
}

} // namespace
