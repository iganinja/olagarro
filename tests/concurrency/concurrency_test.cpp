#include <concurrency/concurrency.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
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
