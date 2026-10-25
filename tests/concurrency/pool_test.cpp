#include <concurrency/pool.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace olagarro;
using namespace olagarro::concurrency;

namespace
{

constexpr std::chrono::seconds long_enough{10};

// Keeps a pool's only worker busy until let go, so what is submitted after it waits in the queue.
class Blocker
{
  public:
    explicit Blocker(TaskPool& pool)
        : m_handle{pool.submit(
              [started{&m_started}, release{m_release.get_future().share()}](const CancelToken&)
              {
                  started->set_value();
                  release.wait();
              })}
    {
        m_started.get_future().wait();
    }

    ~Blocker() { release(); }

    void release()
    {
        if(!m_released)
        {
            m_released = true;
            m_release.set_value();
            m_handle.wait();
        }
    }

  private:
    std::promise<void> m_started;
    std::promise<void> m_release;
    bool m_released{false};
    TaskHandle<void> m_handle;
};

} // namespace

TEST(TaskPoolTests, StartsAsManyWorkersAsAsked)
{
    TaskPool pool{3};
    EXPECT_EQ(pool.worker_count(), 3u);
    TaskPool hardware;
    EXPECT_GE(hardware.worker_count(), 1u);
}

TEST(TaskPoolTests, RunsWorkAndGivesItsResult)
{
    TaskPool pool{2};
    auto handle{pool.submit([](const CancelToken&) { return std::string{"done"}; })};
    EXPECT_EQ(handle.get(), "done");
    EXPECT_TRUE(handle.is_finished());
}

TEST(TaskPoolTests, RunsWorkOffTheCallersThread)
{
    TaskPool pool{1};
    auto handle{pool.submit([](const CancelToken&) { return std::this_thread::get_id(); })};
    EXPECT_NE(handle.get(), std::this_thread::get_id());
}

TEST(TaskPoolTests, GivesWhatTheWorkThrewThroughItsResult)
{
    TaskPool pool{1};
    auto handle{pool.submit([](const CancelToken&) -> int { throw std::runtime_error{"expected test exception"}; })};
    EXPECT_THROW(handle.get(), std::runtime_error);
}

TEST(TaskPoolTests, IsNotFinishedWhileTheWorkRuns)
{
    TaskPool pool{1};
    Blocker blocker{pool};
    auto handle{pool.submit([](const CancelToken&) {})};
    EXPECT_FALSE(handle.is_finished());
    blocker.release();
    handle.wait();
    EXPECT_TRUE(handle.is_finished());
}

TEST(TaskPoolTests, ACancelledTaskStopsAtItsNextCheck)
{
    TaskPool pool{1};
    std::promise<void> started;
    std::atomic<int> checks{0};
    auto handle{pool.submit(
        [&](const CancelToken& token)
        {
            started.set_value();
            while(!token.cancelled())
            {
                ++ checks;
                std::this_thread::yield();
            }
            return checks.load();
        })};
    started.get_future().wait();
    handle.cancel();
    ASSERT_EQ(handle.future().wait_for(long_enough), std::future_status::ready);
    EXPECT_EQ(handle.get(), checks.load());
    EXPECT_TRUE(handle.token().cancelled());
}

TEST(TaskPoolTests, ATaskCancelledBeforeItStartsNeverRuns)
{
    TaskPool pool{1};
    Blocker blocker{pool};
    std::atomic<bool> ran{false};
    auto handle{pool.submit([&](const CancelToken&) { ran = true; })};
    handle.cancel();
    blocker.release();
    EXPECT_THROW(handle.get(), TaskCancelled);
    EXPECT_FALSE(ran.load());
}

TEST(TaskPoolTests, RunsTasksInTheOrderGiven)
{
    TaskPool pool{1};
    std::vector<int> order;
    std::vector<TaskHandle<void>> handles;
    {
        Blocker blocker{pool};
        for(auto index{0}; index < 5; ++ index)
        {
            handles.push_back(pool.submit([&order, index](const CancelToken&) { order.push_back(index); }));
        }
    }
    for(auto& handle : handles)
    {
        handle.get();
    }
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST(TaskPoolTests, DropsWhatHasNotStartedWhenDestroyed)
{
    std::atomic<bool> ran{false};
    TaskHandle<void> waiting;
    std::promise<void> release;
    std::thread releaser;
    {
        TaskPool pool{1};
        std::promise<void> started;
        auto running{pool.submit(
            [&started, gate{release.get_future().share()}](const CancelToken&)
            {
                started.set_value();
                gate.wait();
            })};
        started.get_future().wait();
        waiting = pool.submit([&](const CancelToken&) { ran = true; });
        // The running task is let go only once the pool, being destroyed, has dropped the waiting one.
        releaser = std::thread{[&]
                               {
                                   while(!waiting.is_finished())
                                   {
                                       std::this_thread::yield();
                                   }
                                   release.set_value();
                               }};
    }
    releaser.join();
    EXPECT_THROW(waiting.get(), TaskCancelled);
    EXPECT_FALSE(ran.load());
}

TEST(TaskPoolTests, WorkersRunAtTheSameTime)
{
    TaskPool pool{2};
    std::promise<void> first_started;
    std::promise<void> second_started;
    auto first{pool.submit(
        [&, other{second_started.get_future().share()}](const CancelToken&)
        {
            first_started.set_value();
            return other.wait_for(long_enough) == std::future_status::ready;
        })};
    auto second{pool.submit(
        [&, other{first_started.get_future().share()}](const CancelToken&)
        {
            second_started.set_value();
            return other.wait_for(long_enough) == std::future_status::ready;
        })};
    EXPECT_TRUE(first.get());
    EXPECT_TRUE(second.get());
}

TEST(TaskPoolTests, TheSharedPoolIsOneForTheWholeProgram)
{
    EXPECT_EQ(&TaskPool::shared(), &TaskPool::shared());
    EXPECT_GE(TaskPool::shared().worker_count(), 1u);
}

TEST(CancelTokenTests, CopiesShareTheirCancelling)
{
    CancelToken token;
    const CancelToken copy{token};
    EXPECT_FALSE(copy.cancelled());
    token.cancel();
    EXPECT_TRUE(copy.cancelled());
    EXPECT_FALSE(CancelToken{}.cancelled());
}

TEST(LatestWinsTests, OfTenQuickRequestsOnlyTheLastFinishes)
{
    TaskPool pool{2};
    std::mutex mutex;
    std::vector<int> results;
    std::atomic<int> started{0};
    std::promise<void> last_delivered;
    LatestWins<int, int> latest{pool,
                                [&](const int& request, const CancelToken& token)
                                {
                                    ++ started;
                                    // Long enough that the next request comes while this one runs.
                                    const auto until{std::chrono::steady_clock::now() + std::chrono::milliseconds{20}};
                                    while(!token.cancelled() && std::chrono::steady_clock::now() < until)
                                    {
                                        std::this_thread::yield();
                                    }
                                    return request * 10;
                                },
                                [&](int result)
                                {
                                    const std::lock_guard<std::mutex> lock{mutex};
                                    results.push_back(result);
                                    if(result == 90)
                                    {
                                        last_delivered.set_value();
                                    }
                                }};
    for(auto request{0}; request < 10; ++ request)
    {
        latest.request(request);
    }
    ASSERT_EQ(last_delivered.get_future().wait_for(long_enough), std::future_status::ready);
    latest.wait();
    const std::lock_guard<std::mutex> lock{mutex};
    EXPECT_EQ(results, (std::vector<int>{90}));
    EXPECT_LE(started.load(), 10);
}

TEST(LatestWinsTests, ARequestAfterTheLastFinishedIsHandedToo)
{
    TaskPool pool{1};
    std::mutex mutex;
    std::vector<std::string> results;
    LatestWins<std::string, std::string> latest{pool, [](const std::string& request, const CancelToken&)
                                                { return request + "!"; },
                                                [&](std::string result)
                                                {
                                                    const std::lock_guard<std::mutex> lock{mutex};
                                                    results.push_back(std::move(result));
                                                }};
    latest.request("one");
    latest.wait();
    latest.request("two");
    latest.wait();
    const std::lock_guard<std::mutex> lock{mutex};
    EXPECT_EQ(results, (std::vector<std::string>{"one!", "two!"}));
}

TEST(LatestWinsTests, DestroyingItCancelsAndWaitsForItsWork)
{
    TaskPool pool{1};
    std::atomic<bool> stopped{false};
    std::atomic<int> handed{0};
    std::promise<void> started;
    {
        LatestWins<int, int> latest{pool,
                                    [&](const int&, const CancelToken& token)
                                    {
                                        started.set_value();
                                        while(!token.cancelled())
                                        {
                                            std::this_thread::yield();
                                        }
                                        stopped = true;
                                        return 0;
                                    },
                                    [&](int) { ++ handed; }};
        latest.request(1);
        started.get_future().wait();
    }
    EXPECT_TRUE(stopped.load());
    EXPECT_EQ(handed.load(), 0);
}
