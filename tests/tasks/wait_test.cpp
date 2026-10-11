
#include <tasks/taskexecutor.h>
#include <tasks/wait.h>
#include <tasks/serial.h>
#include <tasks/execute.h>
#include <tasks/repeat.h>

#include <gtest/gtest.h>
#include <chrono>
#include <random>

using namespace olagarro;
using namespace olagarro::tasks;

namespace
{

TEST(WaitTest, WaitWaitsProvidedTime)
{
    auto value{0};
    const auto target_value{rand()};

    TaskExecutor TE;

    TE.add_task(serial(wait(1.0f), execute([&value, target_value]() { value = target_value; })));

    EXPECT_EQ(value, 0);

    TE.update(0.5f);

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, target_value);
}

TEST(WaitTest, WaitWaitsForAPredicate)
{
    auto value{0};
    constexpr auto Times{10};
    const auto target_value{rand()};

    TaskExecutor TE;

    TE.add_task(
        serial(wait([&]() { return value < Times; }), execute([&value, target_value]() { value = target_value; })));

    EXPECT_EQ(value, 0);

    for(auto i{0u}; i < Times * 4; ++ i)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(value, target_value);
}

TEST(WaitTest, WaitHeldFinishesWhenThePredicateHoldsLongEnoughInARow)
{
    bool holds{true};

    TaskExecutor TE;
    TE.add_task(wait_held([&] { return holds; }, 1.0f));

    TE.update(0.75f);
    EXPECT_FALSE(TE.is_empty());

    holds = false;
    TE.update(0.75f);
    EXPECT_FALSE(TE.is_empty());

    holds = true;
    TE.update(0.75f);
    EXPECT_FALSE(TE.is_empty());

    TE.update(0.25f);
    EXPECT_TRUE(TE.is_empty());
}

TEST(WaitTest, WaitForAsksForTheDurationOnEveryStart)
{
    int asked{0};
    int finished{0};

    TaskExecutor TE;
    TE.add_task(repeat_forever(serial(wait_for(
                                          [&]
                                          {
                                              ++ asked;
                                              return static_cast<float>(asked);
                                          }),
                                      execute([&] { ++ finished; }))));

    EXPECT_EQ(asked, 0);

    TE.update(1.0f);
    EXPECT_EQ(asked, 1);
    EXPECT_EQ(finished, 1);

    TE.update(1.0f);
    EXPECT_EQ(asked, 2);
    EXPECT_EQ(finished, 1);

    TE.update(1.0f);
    EXPECT_EQ(finished, 2);
}

TEST(WaitTest, WaitRandomDrawsItsDurationOnEveryStart)
{
    std::mt19937 generator{1234u};
    std::mt19937 expected_generator{generator};
    const auto first{std::uniform_real_distribution<float>{1.0f, 3.0f}(expected_generator)};
    int finished{0};

    TaskExecutor TE;
    TE.add_task(repeat_forever(serial(wait_random(1.0f, 3.0f, generator), execute([&] { ++ finished; }))));

    TE.update(first * 0.5f);
    TE.update(first * 0.25f);
    EXPECT_EQ(finished, 0);
    EXPECT_TRUE(generator == expected_generator);

    TE.update(first * 0.25f);
    EXPECT_EQ(finished, 1);

    // The next update starts it again, and draws again.
    TE.update(0.0f);
    static_cast<void>(std::uniform_real_distribution<float>{1.0f, 3.0f}(expected_generator));
    EXPECT_TRUE(generator == expected_generator);
}

} // namespace
