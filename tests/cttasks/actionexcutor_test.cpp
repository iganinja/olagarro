#include <cttasks/taskexecutor.h>
#include <cttasks/serial.h>
#include <cttasks/parallel.h>
#include "cttasks/earlyexitparallel.h"
#include "cttasks/repeat.h"
#include <cttasks/execute.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

#include <iostream>

using namespace Olagarro::CTTasks;
using ::testing::Exactly;
using ::testing::Return;
using ::testing::_;

namespace
{

class TaskExecutorTests : public ::testing::Test
{
void SetUp() override
{
    srand(time(NULL));
}

};

class ValueTestTask
{
public:
    ValueTestTask(int& value) :
        mValue{value}
    {
    }

    void onAboutToStart()
    {
        ++ mValue;
    }

    State update(float deltaTime)
    {
        return State::Finished;
    }
private:
    int& mValue;
};

TEST_F(TaskExecutorTests, ActionExecutorCallsOnAboutToStart)
{
    std::array<int, 3> values{10, 20, 30};

    TaskExecutor TE{
                    ValueTestTask{values[0]},
                    ValueTestTask{values[1]},
                    ValueTestTask{values[2]}
                 };

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 21);
    EXPECT_EQ(values[2], 31);
}

}
