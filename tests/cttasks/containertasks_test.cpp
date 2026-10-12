#include <cttasks/taskexecutor.h>
#include <cttasks/serial.h>
#include <cttasks/parallel.h>
#include "cttasks/earlyexitparallel.h"
#include "cttasks/repeat.h"
#include <cttasks/execute.h>
#include <cttasks/select.h>
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

class ContainerTasksTests : public ::testing::Test
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

TEST_F(ContainerTasksTests, SerialExecutesTasksInASerialWay)
{
    auto value{0};

    TaskExecutor TE{
        serial(execute([&]() { value = 1; }), execute([&]() { value = 2; }), execute([&]() { value = 3; }))};

    EXPECT_EQ(value, 0);

    TE.update();

    EXPECT_EQ(value, 3);
}

TEST_F(ContainerTasksTests, SerialCallsOnAboutToStartOnAllTasks)
{
    std::array<int, 3> values{10, 20, 30};

    TaskExecutor TE{serial(ValueTestTask{values[0]}, ValueTestTask{values[1]}, ValueTestTask{values[2]})};

    TE.update();

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 21);
    EXPECT_EQ(values[2], 31);
}

TEST_F(ContainerTasksTests, ParallelExecutesTasksInAParallelWay)
{
    std::array<int, 3> values{0, 0, 0};

    TaskExecutor TE{parallel(execute([&]() { values[0] = 1; }), execute([&]() { values[1] = 2; }),
                             execute([&]() { values[2] = 3; }))};

    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[1], 0);
    EXPECT_EQ(values[2], 0);

    TE.update();

    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST_F(ContainerTasksTests, ParallelCallsOnAboutToStartOnAllTasks)
{
    std::array<int, 3> values{10, 20, 30};

    TaskExecutor TE{parallel(ValueTestTask{values[0]}, ValueTestTask{values[1]}, ValueTestTask{values[2]})};

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 21);
    EXPECT_EQ(values[2], 31);
}

TEST_F(ContainerTasksTests, EarlyExitParallelSkipsWithFirstFinishedTask)
{
    auto value{0};
    const auto target_value{rand()};

    TaskExecutor TE{
        serial(early_exit_parallel(execute([&]() { value = target_value + 1; }), repeat_forever(execute([]() {}))),
               execute([&]() { value = target_value; }))};

    EXPECT_EQ(value, 0);

    TE.update();

    EXPECT_EQ(value, target_value);
}

TEST_F(ContainerTasksTests, EarlyExitParallelCallsOnAboutToStartOnAllTasks)
{
    std::array<int, 3> values{10, 20, 30};

    TaskExecutor TE{early_exit_parallel(ValueTestTask{values[0]}, ValueTestTask{values[1]}, ValueTestTask{values[2]})};

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 21);
    EXPECT_EQ(values[2], 31);
}

TEST_F(ContainerTasksTests, SelectChoosesTasks)
{
    auto value{0};

    TaskExecutor TE{select(select_entry(wait([]() { return true; }), execute([&]() { value = 1; })),
                           select_entry(wait([]() { return false; }), execute([&]() { value = 2; })))};

    EXPECT_EQ(value, 0);

    TE.update();

    EXPECT_EQ(value, 0);

    TE.update();

    EXPECT_EQ(value, 1);
}

TEST_F(ContainerTasksTests, SelectCallsOnAboutToStartInAllPredicatesAndTheSelectedTask)
{
    class SelectTestTask
    {
      public:
        SelectTestTask(bool shouldFinish, int& value, int target_value)
            : m_should_finish{shouldFinish}, m_value{value}, m_target_value{target_value}
        {
        }

        void on_about_to_start() { m_value = m_target_value; }

        State update(float) { return m_should_finish ? State::finished : State::working; }

      private:
        bool m_should_finish;
        int& m_value;
        const int m_target_value;
    };

    auto value{0};

    TaskExecutor TE{select(SelectEntry{SelectTestTask{false, value, 10}, SelectTestTask{false, value, 100}},
                           SelectEntry{SelectTestTask{true, value, 20}, SelectTestTask{false, value, 200}},
                           SelectEntry{SelectTestTask{false, value, 30}, SelectTestTask{false, value, 300}})};

    EXPECT_EQ(value, 30);

    for(auto i{0u}; i < 6u; ++ i)
    {
        TE.update();
    }

    EXPECT_EQ(value, 200);
}

TEST_F(ContainerTasksTests, RepeatRepeatsTaskNTimes)
{
    constexpr auto Times{5u};
    auto value{0};

    TaskExecutor TE{repeat(execute(
                               [&]()
                               {
                                   ++ value;
                                   std::cout << "value = " << value << "\n";
                               }),
                           Times)};

    EXPECT_EQ(value, 0);

    for(auto i{0u}; i < Times * 2; ++ i)
    {
        TE.update();
    }

    EXPECT_EQ(value, static_cast<int>(Times));
}

TEST_F(ContainerTasksTests, RepeatCallsOnAboutToStartNTimes)
{
    constexpr auto Times{5};
    auto value{0};

    TaskExecutor TE{repeat(ValueTestTask{value}, Times)};

    for(auto i{0}; i < Times * 2; ++ i)
    {
        TE.update();
    }

    EXPECT_EQ(value, Times);
}

TEST_F(ContainerTasksTests, RepeatWhileChecksThePredicate)
{
    const auto times{rand() % 20};
    auto value{0};

    TaskExecutor TE{repeat_while([&]() { return value < times; }, execute([&]() { ++ value; }))};

    for(auto i{0}; i < times * 2; ++ i)
    {
        TE.update();
    }

    EXPECT_EQ(value, times);
}

TEST_F(ContainerTasksTests, RepeatWhileCallsOnAboutToStartCorrectNumberOfTimes)
{
    const int times{rand() % 20};
    auto value{0};

    TaskExecutor TE{repeat_while([&]() { return value < times; }, ValueTestTask{value})};

    for(auto i{0}; i < times * 2; ++ i)
    {
        TE.update();
    }

    EXPECT_EQ(value, times);
}

} // namespace
