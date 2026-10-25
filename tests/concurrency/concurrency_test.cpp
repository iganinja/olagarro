#include <concurrency/concurrency.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

using namespace olagarro;
using namespace olagarro::concurrency;

TEST(ConcurrencyTests, AcceptsEmptyRanges)
{
    std::vector<int> values{};
    EXPECT_NO_THROW(concurrent_for(values.begin(), values.end(), [](int, int&) {}).get());
}

TEST(ConcurrencyTests, ReducesWorkerLocalResults)
{
    std::vector<int> values{1, 2, 3, 4};
    auto result{concurrent_for(
        values.begin(), values.end(), [](int, const int& value) { return value; },
        reductor(0, [](int accumulated, int value) { return accumulated + value; }))};

    EXPECT_EQ(result.get(), 10);
}

TEST(ConcurrencyTests, PropagatesWorkerExceptionsThroughTheFuture)
{
    std::vector<int> values{1};
    auto result{concurrent_for(values.begin(), values.end(),
                               [](int, int&) { throw std::runtime_error{"expected test exception"}; })};

    EXPECT_THROW(result.get(), std::runtime_error);
}

TEST(ConcurrencyTests, CountsTheInitialValueOnceWhateverTheWorkers)
{
    std::vector<int> values(100);
    for(std::size_t index{}; index < values.size(); ++ index)
    {
        values[index] = static_cast<int>(index) + 1;
    }
    // 1 + 2 + ... + 100 is 5050; the initial 10 counts once, not once per worker.
    auto result{concurrent_for(
        values.begin(), values.end(), [](int, int value) { return value; },
        reductor(10, [](int accumulated, int value) { return accumulated + value; }))};

    EXPECT_EQ(result.get(), 5060);
}

// The index came as an int, which a range longer than INT_MAX overflows.
TEST(ConcurrencyTests, GivesTheIndexAsASize)
{
    std::vector<int> values{5, 6, 7};
    std::vector<std::size_t> seen(values.size());
    concurrent_for(values.begin(), values.end(),
                   [&seen](auto index, int& value)
                   {
                       static_assert(std::is_same_v<decltype(index), std::size_t>);
                       seen[index] = static_cast<std::size_t>(value);
                   })
        .get();
    EXPECT_EQ(seen, (std::vector<std::size_t>{5, 6, 7}));
}

TEST(ConcurrencyTests, RunsOnThePoolGivenAndTheCallersThread)
{
    TaskPool pool{2};
    std::vector<int> values(1000);
    std::vector<std::thread::id> threads(values.size());
    concurrent_for(pool, values.begin(), values.end(),
                   [&threads](std::size_t index, int&) { threads[index] = std::this_thread::get_id(); })
        .get();
    std::sort(threads.begin(), threads.end());
    threads.erase(std::unique(threads.begin(), threads.end()), threads.end());
    // At most the pool's two workers and the caller, who helps while it waits.
    EXPECT_GE(threads.size(), 1u);
    EXPECT_LE(threads.size(), 3u);
}

TEST(ConcurrencyTests, TheCallerDoesTheWorkWhenEveryWorkerIsBusy)
{
    TaskPool pool{1};
    std::promise<void> started;
    std::promise<void> release;
    auto busy{pool.submit(
        [&started, gate{release.get_future().share()}](const CancelToken&)
        {
            started.set_value();
            gate.wait();
        })};
    started.get_future().wait();

    std::vector<int> values{1, 2, 3, 4, 5};
    const auto sum{concurrent_for(
                       pool, values.begin(), values.end(), [](std::size_t, int value) { return value; },
                       reductor(0, [](int accumulated, int value) { return accumulated + value; }))
                       .get()};
    EXPECT_EQ(sum, 15);
    release.set_value();
    busy.get();
}

// Every worker waiting on a concurrent_for of its own would wait forever if waiting did not help.
TEST(ConcurrencyTests, ConcurrentForInsidePoolTasksDoesNotWaitForever)
{
    TaskPool pool{2};
    std::vector<TaskHandle<int>> handles;
    for(auto task{0}; task < 6; ++ task)
    {
        handles.push_back(pool.submit(
            [&pool](const CancelToken&)
            {
                std::vector<int> values(100, 1);
                return concurrent_for(
                           pool, values.begin(), values.end(), [](std::size_t, int value) { return value; },
                           reductor(0, [](int accumulated, int value) { return accumulated + value; }))
                    .get();
            }));
    }
    for(auto& handle : handles)
    {
        ASSERT_EQ(handle.future().wait_for(std::chrono::seconds{10}), std::future_status::ready);
        EXPECT_EQ(handle.get(), 100);
    }
}

TEST(ConcurrencyTests, RunsOnTheSharedPoolWhenGivenNone)
{
    const auto shared_workers{TaskPool::shared().worker_count()};
    std::vector<int> values(1000);
    std::vector<std::thread::id> threads(values.size());
    concurrent_for(values.begin(), values.end(),
                   [&threads](std::size_t index, int&) { threads[index] = std::this_thread::get_id(); })
        .get();
    std::sort(threads.begin(), threads.end());
    threads.erase(std::unique(threads.begin(), threads.end()), threads.end());
    EXPECT_LE(threads.size(), shared_workers + 1u);
}

TEST(ConcurrencyTests, AnExceptionStopsTheOtherShares)
{
    TaskPool pool{2};
    std::vector<int> values(100000);
    std::atomic<int> visited{0};
    auto result{concurrent_for(pool, values.begin(), values.end(),
                               [&visited](std::size_t, int&)
                               {
                                   ++ visited;
                                   throw std::runtime_error{"expected test exception"};
                               })};
    EXPECT_THROW(result.get(), std::runtime_error);
    EXPECT_LT(visited.load(), 100000);
}
