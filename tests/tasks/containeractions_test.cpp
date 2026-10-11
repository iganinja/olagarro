
#include <tasks/taskexecutor.h>
#include <tasks/serial.h>
#include <tasks/parallel.h>
#include <tasks/earlyexitparallel.h>
#include <tasks/execute.h>
#include <tasks/neverfinish.h>
#include <tasks/select.h>
#include <tasks/repeat.h>
#include <tasks/wait.h>

#include "probe.h"
#include "task_mock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

using namespace olagarro;
using namespace olagarro::tasks;
using ::testing::_;
using ::testing::Exactly;
using ::testing::Return;

namespace
{

std::unique_ptr<Mocks::TaskMock> taskMock()
{
    return std::make_unique<Mocks::TaskMock>();
}

class ContainerActionsTests : public ::testing::Test
{
    void SetUp() override { srand(time(NULL)); }
};

TEST_F(ContainerActionsTests, SerialExecutesTasksInASerialWay)
{
    TaskExecutor TE;

    std::size_t value{0};

    TE.add_task(serial(execute([&]() { value = 1; }), execute([&]() { value = 2; }), execute([&]() { value = 3; })));

    EXPECT_EQ(value, 0);

    TE.update(0.1f);

    EXPECT_EQ(value, 3);
}

TEST_F(ContainerActionsTests, SerialCallsOnAboutToStartOnAllTasks)
{
    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*task1.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*task1.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::finished));

    EXPECT_CALL(*task2.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*task2.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::finished));

    EXPECT_CALL(*task3.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*task3.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::finished));

    TaskExecutor TE;

    TE.add_task(serial(std::move(task1), std::move(task2), std::move(task3)));

    TE.update(1.0f);
}

TEST_F(ContainerActionsTests, ParallelExecutesTasksInAParallelWay)
{
    TaskExecutor TE;

    auto value1{0};
    auto value2{0};
    auto value3{0};

    TE.add_task(
        parallel(execute([&]() { value1 = 1; }), execute([&]() { value2 = 1; }), execute([&]() { value3 = 1; })));

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

    EXPECT_CALL(*task1.get(), on_about_to_start()).Times(Exactly(1));

    EXPECT_CALL(*task2.get(), on_about_to_start()).Times(Exactly(1));

    EXPECT_CALL(*task3.get(), on_about_to_start()).Times(Exactly(1));

    TaskExecutor TE;

    TE.add_task(parallel(std::move(task1), std::move(task2), std::move(task3)));

    TE.update(1.0f);
}

TEST_F(ContainerActionsTests, EarlyExitParallelSkipsWithFirstFinishedTask)
{
    TaskExecutor TE;

    auto value{0};
    const auto target_value{rand()};

    TE.add_task(early_exit_parallel(execute([&]() { value = target_value; }), repeat_forever(execute([]() {}))));

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, target_value);
}

TEST_F(ContainerActionsTests, ASidecarThatFinishesFirstDoesNotEndTheGroup)
{
    const auto task{std::make_shared<Probes::ProbeLog>()};
    const auto sidecar{std::make_shared<Probes::ProbeLog>()};
    sidecar->finishes = true;

    TaskExecutor TE;
    TE.add_task(early_exit_parallel_with_sidecars(task_list(Probes::probe(task)), task_list(Probes::probe(sidecar))));

    TE.update(1.0f);
    TE.update(1.0f);
    EXPECT_FALSE(TE.is_empty());
    EXPECT_EQ(task->updates, 2);

    task->finishes = true;
    TE.update(1.0f);
    EXPECT_TRUE(TE.is_empty());
}

TEST_F(ContainerActionsTests, AFinishedSidecarIsNotUpdatedAgain)
{
    const auto sidecar{std::make_shared<Probes::ProbeLog>()};
    const auto other_sidecar{std::make_shared<Probes::ProbeLog>()};
    sidecar->finishes = true;

    TaskExecutor TE;
    TE.add_task(early_exit_parallel_with_sidecars(task_list(repeat_forever(execute([] {}))),
                                                  task_list(Probes::probe(sidecar), Probes::probe(other_sidecar))));

    for(int index{0}; index < 4; ++ index)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(sidecar->starts, 1);
    EXPECT_EQ(sidecar->updates, 1);
    EXPECT_EQ(other_sidecar->updates, 4);
}

TEST_F(ContainerActionsTests, AnEarlyExitParallelHandsOverItsSidecarsToo)
{
    auto group{early_exit_parallel_with_sidecars(task_list(execute([] {}), execute([] {})),
                                                 task_list(execute([] {}), nullptr, execute([] {})))};

    EXPECT_EQ(group->children().size(), 4u);
    EXPECT_EQ(group->grab_children().size(), 4u);
    EXPECT_TRUE(group->children().empty());
}

TEST_F(ContainerActionsTests, NeverFinishKeepsWorkingAfterItsTaskIsDone)
{
    int calls{0};

    TaskExecutor TE;
    TE.add_task(early_exit_parallel(never_finish(execute([&] { ++ calls; })), wait(3.0f)));

    TE.update(1.0f);
    TE.update(1.0f);
    EXPECT_FALSE(TE.is_empty());
    EXPECT_EQ(calls, 1);

    TE.update(1.0f);
    EXPECT_TRUE(TE.is_empty());
    EXPECT_EQ(calls, 1);
}

TEST_F(ContainerActionsTests, EarlyExitParallelCallsOnAboutToStartOnAllTasks)
{
    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*task1.get(), on_about_to_start()).Times(Exactly(1));

    EXPECT_CALL(*task2.get(), on_about_to_start()).Times(Exactly(1));

    EXPECT_CALL(*task3.get(), on_about_to_start()).Times(Exactly(1));

    TaskExecutor TE;

    TE.add_task(early_exit_parallel(std::move(task1), std::move(task2), std::move(task3)));

    for(auto i{0u}; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, SelectChoosesTasks)
{
    TaskExecutor TE;

    auto value{0};

    TE.add_task(select(SelectEntry{wait([]() { return true; }), execute([&]() { value = 1; })},
                       SelectEntry{wait([]() { return false; }), execute([&]() { value = 2; })}));

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, 1);
}

// When several predicates hold on the same update, the first entry wins. It used to be the last one, after the first
// task had already been started and then dropped.
TEST_F(ContainerActionsTests, SelectPicksTheFirstEntryWhenSeveralHoldTogether)
{
    TaskExecutor TE;

    auto first_runs{0};
    auto second_runs{0};

    TE.add_task(select(SelectEntry{wait([]() { return true; }), execute([&]() { ++ first_runs; })},
                       SelectEntry{wait([]() { return true; }), execute([&]() { ++ second_runs; })}));

    TE.update(1.0f);
    TE.update(1.0f);

    EXPECT_EQ(first_runs, 1);
    EXPECT_EQ(second_runs, 0);
}

TEST_F(ContainerActionsTests, SelectCallsOnAboutToStartInAllPredicatesAndTheSelectedTask)
{
    auto predicate1{taskMock()};
    auto predicate2{taskMock()};
    auto predicate3{taskMock()};

    auto task1{taskMock()};
    auto task2{taskMock()};
    auto task3{taskMock()};

    EXPECT_CALL(*predicate1.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*predicate1.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::finished));
    EXPECT_CALL(*task1.get(), on_about_to_start()).Times(Exactly(1));

    EXPECT_CALL(*predicate2.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*predicate2.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::working));

    EXPECT_CALL(*predicate3.get(), on_about_to_start()).Times(Exactly(1));
    EXPECT_CALL(*predicate3.get(), update(_)).Times(Exactly(1)).WillOnce(Return(Task::State::working));

    TaskExecutor TE;

    TE.add_task(select(SelectEntry{std::move(predicate1), std::move(task1)},
                       SelectEntry{std::move(predicate2), std::move(task2)},
                       SelectEntry{std::move(predicate3), std::move(task3)}));

    for(auto i{0u}; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, RepeatRepeatsTaskNTimes)
{
    TaskExecutor TE;

    auto value{0};

    constexpr auto Times{5u};

    TE.add_task(repeat(execute([&]() { ++ value; }), Times));

    EXPECT_EQ(value, 0);

    for(auto i{0u}; i < Times * 2; ++ i)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(value, Times);
}

// Once its task had finished, RepeatWhile started it again on every update after, so a task that takes more than one
// update, as a wait, never finished a second time.
TEST_F(ContainerActionsTests, RepeatForeverStartsItsTaskAgainOnlyAfterItFinishes)
{
    const auto task{std::make_shared<Probes::ProbeLog>()};
    task->finishes = true;

    TaskExecutor TE;
    TE.add_task(repeat_forever(Probes::probe(task)));

    TE.update(1.0f);
    EXPECT_EQ(task->starts, 1);

    task->finishes = false;
    for(int index{0}; index < 4; ++ index)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(task->starts, 2);
    EXPECT_EQ(task->updates, 5);
}

// repeat counted updates rather than runs: a task that takes more than one update ran fewer times than asked.
TEST_F(ContainerActionsTests, RepeatRunsItsTaskTheTimesAskedHoweverLongEachRunTakes)
{
    int runs{0};

    TaskExecutor TE;
    TE.add_task(repeat(serial(wait(3.0f), execute([&] { ++ runs; })), 10));

    for(int second{1}; second < 30; ++ second)
    {
        TE.update(1.0f);
    }
    EXPECT_EQ(runs, 9);
    EXPECT_FALSE(TE.is_empty());

    TE.update(1.0f);
    EXPECT_EQ(runs, 10);
    EXPECT_TRUE(TE.is_empty());
}

// Its count was used up the first time: started again, as inside another repeat, it ran no more.
TEST_F(ContainerActionsTests, RepeatCountsAgainEachTimeItStarts)
{
    int runs{0};

    TaskExecutor TE;
    TE.add_task(repeat(repeat(execute([&] { ++ runs; }), 2), 3));

    for(int update{0}; update < 6; ++ update)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(runs, 6);
    EXPECT_TRUE(TE.is_empty());
}

// tree() ran the names together on one line, listed children last first, and stopped at serials, parallels and
// selects, which did not say their children.
TEST_F(ContainerActionsTests, TreeShowsEveryTaskOnALineOfItsOwnInOrder)
{
    auto task{serial(parallel(wait(1.0f), execute([] {})),
                     select(SelectEntry{wait([] { return true; }), repeat(execute([] {}), 2)}))};
    EXPECT_EQ(task->tree(), "Serial\n"
                            "  Parallel\n"
                            "    Wait\n"
                            "    ExecuteCallable\n"
                            "  Select\n"
                            "    WaitCallable\n"
                            "    Repeat\n"
                            "      ExecuteCallable\n");
}

// A null predicate crashed select, and a null task repeat; the containers leave null tasks out, and these do now.
TEST_F(ContainerActionsTests, NullTasksAreLeftOutEverywhere)
{
    int chosen{0};
    TaskExecutor TE;
    TE.add_task(select(SelectEntry{nullptr, execute([&] { chosen = 1; })},
                       SelectEntry{wait([] { return true; }), execute([&] { chosen = 2; })}));
    TE.add_task(repeat(nullptr, 3));
    TE.add_task(repeat_while([] { return true; }, nullptr));
    TE.update(1.0f);
    TE.update(1.0f);
    EXPECT_EQ(chosen, 2);
    EXPECT_TRUE(TE.is_empty()) << "a repeat of nothing has nothing to do";
}

// wait took seconds only as a float: wait(1.0) and wait(1) did not compile.
TEST_F(ContainerActionsTests, WaitTakesSecondsAsAnyNumber)
{
    TaskExecutor TE;
    TE.add_task(wait(1.0));
    TE.add_task(wait(2));
    TE.update(1.0f);
    EXPECT_FALSE(TE.is_empty());
    TE.update(1.0f);
    EXPECT_TRUE(TE.is_empty());
}

// A serial gave the next task the whole of the update the one before it finished in, so the time counted twice: two
// waits of a second ended together in an update of a second and a half.
TEST_F(ContainerActionsTests, ASerialStartsTheNextTaskWithNoTimeOfItsOwn)
{
    auto done{false};
    TaskExecutor TE;
    TE.add_task(serial(wait(1.0f), wait(1.0f), execute([&done] { done = true; })));
    TE.update(1.5f);
    EXPECT_FALSE(done);
    TE.update(1.0f);
    EXPECT_TRUE(done) << "and what follows a wait still runs in the update it ends";
}

TEST_F(ContainerActionsTests, RepeatCallsOnAboutToStartNTimes)
{
    constexpr auto Times{5u};

    auto task{taskMock()};

    EXPECT_CALL(*task.get(), on_about_to_start()).Times(Exactly(Times));
    EXPECT_CALL(*task.get(), update(_)).Times(Exactly(Times)).WillRepeatedly(Return(Task::State::finished));

    TaskExecutor TE;

    TE.add_task(repeat(std::move(task), Times));

    for(auto i{0u}; i < 6u; ++ i)
    {
        TE.update(1.0f);
    }
}

TEST_F(ContainerActionsTests, RepeatWhileChecksThePredicate)
{
    TaskExecutor TE;

    std::size_t value{0};
    const auto times{static_cast<size_t>(rand() % 20)};

    TE.add_task(repeat_while([&]() { return value < times; }, execute([&]() { ++ value; })));

    EXPECT_EQ(value, 0);

    for(auto i{0u}; i < times * 2; ++ i)
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

    EXPECT_CALL(*task.get(), on_about_to_start()).Times(Exactly(Times));

    EXPECT_CALL(*task.get(), update(_)).Times(Exactly(Times)).WillRepeatedly(Return(Task::State::finished));

    TaskExecutor TE;

    TE.add_task(repeat_while([&]() { return value < Times; }, std::move(task)));

    for(auto i{0u}; i < Times * 2; ++ i)
    {
        TE.update(1.0f);
        ++ value;
    }
}

} // namespace
