#include <tasks/priorityselector.h>
#include <tasks/taskexecutor.h>

#include "probe.h"

#include <gtest/gtest.h>

#include <string>

using namespace olagarro;
using namespace olagarro::tasks;
using Probes::probe;
using Probes::ProbeLog;

namespace
{

std::shared_ptr<ProbeLog> log_that_finishes(bool finishes)
{
    auto log{std::make_shared<ProbeLog>()};
    log->finishes = finishes;
    return log;
}

std::size_t occurrences(const std::string& text, const std::string& word)
{
    std::size_t count{0};

    for(auto found{text.find(word)}; found != std::string::npos; found = text.find(word, found + word.size()))
    {
        ++ count;
    }

    return count;
}

TEST(PrioritySelectorTests, AHigherPriorityEntryPreemptsALowerOneThatStartsAfreshWhenItWinsAgain)
{
    const auto high_predicate{log_that_finishes(false)};
    const auto high_task{log_that_finishes(false)};
    const auto low_predicate{log_that_finishes(true)};
    const auto low_task{log_that_finishes(false)};

    TaskExecutor executor;
    executor.add_task(priority_selector(nullptr, PriorityEntry{probe(high_predicate), probe(high_task)},
                                        PriorityEntry{probe(low_predicate), probe(low_task)}));

    executor.update(1.0f);
    EXPECT_EQ(low_task->starts, 1);
    EXPECT_EQ(low_task->updates, 1);
    EXPECT_EQ(high_task->starts, 0);

    high_predicate->finishes = true;
    const auto low_predicate_updates{low_predicate->updates};
    const auto low_predicate_starts{low_predicate->starts};
    executor.update(1.0f);
    EXPECT_EQ(high_task->starts, 1);
    EXPECT_EQ(high_task->updates, 1);
    EXPECT_EQ(low_task->updates, 1);
    // The entries after the winner are not looked at, and the one it preempted has its predicate started again.
    EXPECT_EQ(low_predicate->updates, low_predicate_updates);
    EXPECT_EQ(low_predicate->starts, low_predicate_starts + 1);

    executor.update(1.0f);
    EXPECT_EQ(high_task->starts, 1);
    EXPECT_EQ(high_task->updates, 2);

    high_predicate->finishes = false;
    executor.update(1.0f);
    EXPECT_EQ(low_task->starts, 2);
    EXPECT_EQ(low_task->updates, 2);
    EXPECT_EQ(high_task->updates, 2);
}

TEST(PrioritySelectorTests, TheDefaultEntryRunsWhenNoPredicateHolds)
{
    const auto predicate{log_that_finishes(false)};
    const auto task{log_that_finishes(false)};
    const auto default_task{log_that_finishes(false)};

    TaskExecutor executor;
    executor.add_task(
        priority_selector(nullptr, PriorityEntry{probe(predicate), probe(task)}, otherwise(probe(default_task))));

    executor.update(1.0f);
    executor.update(1.0f);
    EXPECT_EQ(default_task->starts, 1);
    EXPECT_EQ(default_task->updates, 2);
    EXPECT_EQ(task->starts, 0);

    predicate->finishes = true;
    executor.update(1.0f);
    EXPECT_EQ(task->updates, 1);
    EXPECT_EQ(default_task->updates, 2);
}

TEST(PrioritySelectorTests, TheMainPredicateEndsTheSelectorBeforeAnyEntryRuns)
{
    const auto main_predicate{log_that_finishes(false)};
    const auto task{log_that_finishes(false)};

    TaskExecutor executor;
    executor.add_task(priority_selector(probe(main_predicate), otherwise(probe(task))));

    executor.update(1.0f);
    EXPECT_EQ(main_predicate->starts, 1);
    EXPECT_EQ(task->updates, 1);
    EXPECT_FALSE(executor.is_empty());

    main_predicate->finishes = true;
    executor.update(1.0f);
    EXPECT_EQ(task->updates, 1);
    EXPECT_TRUE(executor.is_empty());
}

TEST(PrioritySelectorTests, WithoutAMainPredicateItNeverFinishes)
{
    const auto task{log_that_finishes(true)};

    TaskExecutor executor;
    executor.add_task(priority_selector(nullptr, otherwise(probe(task))));

    for(int index{0}; index < 10; ++ index)
    {
        executor.update(1.0f);
    }

    EXPECT_FALSE(executor.is_empty());
    EXPECT_EQ(task->updates, 10);
}

TEST(PrioritySelectorTests, AFinishedTaskStartsItsPredicateAgainAndItselfOnItsNextTurn)
{
    const auto predicate{log_that_finishes(true)};
    const auto task{log_that_finishes(false)};

    TaskExecutor executor;
    executor.add_task(priority_selector(nullptr, PriorityEntry{probe(predicate), probe(task)}));

    executor.update(1.0f);
    EXPECT_EQ(predicate->starts, 1);
    EXPECT_EQ(task->starts, 1);

    task->finishes = true;
    executor.update(1.0f);
    EXPECT_EQ(predicate->starts, 2);
    EXPECT_EQ(task->starts, 1);

    task->finishes = false;
    executor.update(1.0f);
    EXPECT_EQ(task->starts, 2);
    EXPECT_EQ(task->updates, 3);
}

TEST(PrioritySelectorTests, ItsTreeShowsEveryTaskItHolds)
{
    auto selector{priority_selector(probe(log_that_finishes(false)),
                                    PriorityEntry{probe(log_that_finishes(false)), probe(log_that_finishes(false))},
                                    otherwise(probe(log_that_finishes(false))))};

    EXPECT_EQ(occurrences(selector->tree(), "Probe"), 4u);
    EXPECT_EQ(selector->grab_children().size(), 5u);
}

} // namespace
