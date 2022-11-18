
#include <tasks/taskexecutor.h>
#include <tasks/wait.h>
#include <tasks/serial.h>
#include <tasks/execute.h>

#include <gtest/gtest.h>
#include <chrono>

using namespace Olagarro::Tasks;

namespace
{

TEST(WaitTest, WaitWaitsProvidedTime)
{
    int value{0};
    const auto targetValue{rand()};

    TaskExecutor TE;

    TE.addTask(
                serial(
                    wait(1.0f),
                    execute([&value, targetValue]()
                    {
                        value = targetValue;
                    })
                )
              );

    EXPECT_EQ(value, 0);

    TE.update(0.5f);

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, 0);

    TE.update(1.0f);

    EXPECT_EQ(value, targetValue);
}

TEST(WaitTest, WaitWaitsForAPredicate)
{
    auto value{0};
    constexpr auto Times{10};
    const auto targetValue{rand()};

    TaskExecutor TE;

    TE.addTask(
                serial(
                        wait([&]()
                        {
                            return value < Times;
                        }),
                        execute([&value, targetValue]()
                        {
                            value = targetValue;
                        })
                )
              );

    EXPECT_EQ(value, 0);

    for(auto i = 0u; i < Times * 4; ++ i)
    {
        TE.update(1.0f);
    }

    EXPECT_EQ(value, targetValue);
}

}
