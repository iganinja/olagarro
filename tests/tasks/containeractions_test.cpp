
#include <tasks/taskexecutor.h>
#include <tasks/serial.h>
#include <tasks/parallel.h>
#include <tasks/earlyexitparallel.h>
#include <tasks/execute.h>
#include <tasks/select.h>
#include <tasks/repeat.h>
#include <tasks/wait.h>

#include "task_mock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

using namespace Olagarro::Tasks;
using ::testing::Exactly;
using ::testing::Return;
using ::testing::_;

namespace
{

std::unique_ptr<Mocks::TaskMock> taskMock()
{
    return std::make_unique<Mocks::TaskMock>();
}

class ContainerActionsTests : public ::testing::Test
{
void SetUp() override
{
    srand(time(NULL));
}
};

TEST_F(ContainerActionsTests, SerialExecutesTasksInASerialWay)
{
    TaskExecutor TE;

    int value{0};

    TE.addTask(
                serial(
                        execute([&]()
                        {
                            value = 1;
                        }),
                        execute([&]()
                        {
                            value = 2;
                        }),
                        execute([&]()
                        {
                            value = 3;
                        })
                      )
              );

    EXPECT_EQ(value, 0);

    TE.update(0.1f);

    EXPECT_EQ(value, 1);

    TE.update(0.1f);

    EXPECT_EQ(value, 2);

    TE.update(0.1f);

    EXPECT_EQ(value, 3);

    TE.update(0.1f);
}

TEST_F(ContainerActionsTests, SerialCallsOnAboutToStartOnAllTasks)
{
    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*task1.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*task1.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Finished));

    EXPECT_CALL(*task2.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*task2.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Finished));

    EXPECT_CALL(*task3.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*task3.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Finished));

    TaskExecutor TE;

    TE.addTask(
                serial(
                        std::move(task1),
                        std::move(task2),
                        std::move(task3)
                      )
              );

    for(auto i = 0u; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, ParallelExecutesTasksInAParallelWay)
{
    TaskExecutor TE;

    int value1{0};
    int value2{0};
    int value3{0};

    TE.addTask(
                parallel(
                            execute([&]()
                            {
                                value1 = 1;
                            }),
                            execute([&]()
                            {
                                value2 = 1;
                            }),
                            execute([&]()
                            {
                                value3 = 1;
                            })
                        )
    );

    EXPECT_EQ(value1, 0);
    EXPECT_EQ(value2, 0);
    EXPECT_EQ(value3, 0);

    TE.update(1.0f);

    EXPECT_EQ(value1, 1);
    EXPECT_EQ(value2, 1);
    EXPECT_EQ(value3, 1);
}

TEST_F(ContainerActionsTests, ParallelCallsOnAboutToStartOnAllTasks)
{
    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*task1.get(), onAboutToStart()).Times(Exactly(1));

    EXPECT_CALL(*task2.get(), onAboutToStart()).Times(Exactly(1));

    EXPECT_CALL(*task3.get(), onAboutToStart()).Times(Exactly(1));

    TaskExecutor TE;

    TE.addTask(
                parallel(
                            std::move(task1),
                            std::move(task2),
                            std::move(task3)
                        )
              );

    TE.update(1.0f);
}

TEST_F(ContainerActionsTests, EarlyExitParallelSkipsWithFirstFinishedTask)
{
    TaskExecutor TE;

    int value{0};
    const int targetValue = rand();

    TE.addTask(
                earlyExitParallel(
                                    execute([&]()
                                    {
                                        value = targetValue;
                                    }),
                                    repeatForever(
                                                    execute([](){})
                                                 )
                                 )
              );

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, targetValue);
}

TEST_F(ContainerActionsTests, EarlyExitParallelCallsOnAboutToStartOnAllTasks)
{
    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*task1.get(), onAboutToStart()).Times(Exactly(1));

    EXPECT_CALL(*task2.get(), onAboutToStart()).Times(Exactly(1));

    EXPECT_CALL(*task3.get(), onAboutToStart()).Times(Exactly(1));

    TaskExecutor TE;

    TE.addTask(
                earlyExitParallel(
                                    std::move(task1),
                                    std::move(task2),
                                    std::move(task3)
                                 )
              );

    for(auto i = 0u; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, SelectChoosesTasks)
{
    TaskExecutor TE;

    int value{0};

    TE.addTask(
                select(
                        SelectEntry{
                            wait([](){ return true; }),
                            execute([&]()
                                    {
                                        value = 1;
                                    })
                        },
                        SelectEntry{
                            wait([](){ return false; }),
                            execute([&]()
                                    {
                                        value = 2;
                                    })
                        }
                      )
              );

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, 1);
}

TEST_F(ContainerActionsTests, SelectCallsOnAboutToStartInAllPredicatesAndTheSelectedTask)
{
    auto predicate1{taskMock()};
    auto predicate2{taskMock()};
    auto predicate3{taskMock()};

    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*predicate1.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*predicate1.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Finished));
    EXPECT_CALL(*task1.get(), onAboutToStart()).Times(Exactly(1));

    EXPECT_CALL(*predicate2.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*predicate2.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Working));

    EXPECT_CALL(*predicate3.get(), onAboutToStart()).Times(Exactly(1));
    EXPECT_CALL(*predicate3.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::Working));


    TaskExecutor TE;

    TE.addTask(
                select(
                        SelectEntry{std::move(predicate1), std::move(task1)},
                        SelectEntry{std::move(predicate2), std::move(task2)},
                        SelectEntry{std::move(predicate3), std::move(task3)}
                      )
              );

    for(auto i = 0u; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, RepeatRepeatsTaskNTimes)
{
    TaskExecutor TE;

    int value{0};

    constexpr auto Times{5u};

    TE.addTask(
                repeat(
                        execute([&]()
                                {
                                    ++ value;
                                }),
                        Times
                      )
              );

    EXPECT_EQ(value, 0);

    for(auto i = 0u; i < Times * 2; ++ i)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(value, Times);
}

TEST_F(ContainerActionsTests, RepeatCallsOnAboutToStartNTimes)
{
    constexpr auto Times{5u};

    auto task{taskMock()};

    EXPECT_CALL(*task.get(), onAboutToStart()).Times(Exactly(Times));
    EXPECT_CALL(*task.get(), update(_)).Times(Exactly(Times)).WillOnce(Return(Task::State::Finished));

    TaskExecutor TE;

    TE.addTask(
                repeat(
                        std::move(task),
                        Times
                      )
              );

    for(auto i = 0u; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, RepeatWhileChecksThePredicate)
{
    TaskExecutor TE;

    int value{0};
    const auto times{static_cast<size_t>(rand() % 20)};

    TE.addTask(
                repeatWhile(
                                [&]()
                                {
                                    return value < times;
                                },
                                execute([&]()
                                {
                                    ++ value;
                                })
                           )
              );

    EXPECT_EQ(value, 0);

    for(auto i = 0u; i < times * 2; ++ i)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(value, times);
}

TEST_F(ContainerActionsTests, RepeatWhileCallsOnAboutToStartCorrectNumberOfTimes)
{
    size_t value{0};
    constexpr auto Times{5u};

    auto task{taskMock()};

    EXPECT_CALL(*task.get(), onAboutToStart()).Times(Exactly(Times));

    EXPECT_CALL(*task.get(), update(_)).Times(Exactly(Times)).WillOnce(Return(Task::State::Finished));

    TaskExecutor TE;

    TE.addTask(
                repeatWhile(
                                [&]()
                                {
                                    return value < Times;
                                },
                                std::move(task)
                           )
              );

    for(auto i = 0u; i < Times * 2; ++ i)
    {
        TE.update(1.0f);
        ++ value;
    }
}

}
