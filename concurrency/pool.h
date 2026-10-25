#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace olagarro::concurrency
{

// What a task's result holds when the task was cancelled, or its pool destroyed, before it started.
class TaskCancelled : public std::exception
{
  public:
    [[nodiscard]] const char* what() const noexcept override { return "task cancelled before it started"; }
};

// A flag that work checks to stop early. Copies share it: cancelling one cancels them all.
class CancelToken
{
  public:
    void cancel() { m_cancelled->store(true); }
    [[nodiscard]] bool cancelled() const { return m_cancelled->load(); }

  private:
    std::shared_ptr<std::atomic<bool>> m_cancelled{std::make_shared<std::atomic<bool>>(false)};
};

// What submitting work to a pool gives back: its token, to cancel it, and the future of its result.
template <typename Result> class TaskHandle
{
  public:
    TaskHandle() = default;
    TaskHandle(std::future<Result> future, CancelToken token) : m_future{std::move(future)}, m_token{std::move(token)}
    {
    }

    // Work not started yet never runs; work running sees it at its next check of the token.
    void cancel() { m_token.cancel(); }

    // True once the work has returned or thrown, or was dropped without running.
    [[nodiscard]] bool is_finished() const
    {
        if(!m_future.valid())
        {
            return m_taken;
        }
        return m_future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
    }

    void wait() const { m_future.wait(); }

    // Waits, and gives the result, rethrows what the work threw, or throws TaskCancelled. Only once.
    Result get()
    {
        m_taken = true;
        return m_future.get();
    }

    [[nodiscard]] std::future<Result>& future() { return m_future; }
    [[nodiscard]] const CancelToken& token() const { return m_token; }

  private:
    std::future<Result> m_future;
    CancelToken m_token;
    bool m_taken{false};
};

// Threads started once, taking the work submitted to them first in, first out.
class TaskPool
{
  public:
    TaskPool() : TaskPool{std::max(1u, std::thread::hardware_concurrency())} {}

    explicit TaskPool(unsigned worker_count)
    {
        m_workers.reserve(worker_count);
        for(auto index{0u}; index < std::max(1u, worker_count); ++ index)
        {
            m_workers.emplace_back([this] { work(); });
        }
    }

    // Waits for the work running; what has not started is dropped, its results throwing TaskCancelled.
    ~TaskPool()
    {
        std::deque<Job> dropped;
        {
            const std::lock_guard<std::mutex> lock{m_mutex};
            m_stopping = true;
            dropped.swap(m_queue);
        }
        m_queue_changed.notify_all();
        for(auto& job : dropped)
        {
            job(false);
        }
        for(auto& worker : m_workers)
        {
            worker.join();
        }
    }

    TaskPool(const TaskPool&) = delete;
    TaskPool& operator=(const TaskPool&) = delete;

    // The program's pool, as many workers as the hardware runs at once, started when first asked for. What shares
    // the cores should share it: concurrent_for runs on it when given no pool.
    [[nodiscard]] static TaskPool& shared()
    {
        static TaskPool pool;
        return pool;
    }

    [[nodiscard]] unsigned worker_count() const { return static_cast<unsigned>(m_workers.size()); }

    // Queues `work(const CancelToken&)`, which returns the task's result and may check the token to stop early.
    template <typename Work> [[nodiscard]] auto submit(Work work)
    {
        using Result = std::invoke_result_t<Work&, const CancelToken&>;
        const CancelToken token;
        auto promise{std::make_shared<std::promise<Result>>()};
        TaskHandle<Result> handle{promise->get_future(), token};
        auto shared_work{std::make_shared<Work>(std::move(work))};
        enqueue(
            [promise, token, shared_work](bool run)
            {
                if(!run || token.cancelled())
                {
                    promise->set_exception(std::make_exception_ptr(TaskCancelled{}));
                    return;
                }
                try
                {
                    if constexpr(std::is_void_v<Result>)
                    {
                        (*shared_work)(token);
                        promise->set_value();
                    }
                    else
                    {
                        promise->set_value((*shared_work)(token));
                    }
                }
                catch(...)
                {
                    promise->set_exception(std::current_exception());
                }
            });
        return handle;
    }

  private:
    // Runs the work with true, or drops it with false.
    using Job = std::function<void(bool run)>;

    void enqueue(Job job)
    {
        {
            const std::lock_guard<std::mutex> lock{m_mutex};
            if(!m_stopping)
            {
                m_queue.push_back(std::move(job));
                job = nullptr;
            }
        }
        if(job)
        {
            job(false);
            return;
        }
        m_queue_changed.notify_one();
    }

    void work()
    {
        while(true)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock{m_mutex};
                m_queue_changed.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
                if(m_queue.empty())
                {
                    return;
                }
                job = std::move(m_queue.front());
                m_queue.pop_front();
            }
            job(true);
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_queue_changed;
    std::deque<Job> m_queue;
    bool m_stopping{false};
    std::vector<std::thread> m_workers;
};

// Runs each request on a pool, cancelling the one before: only the result of a request still the latest when it
// finishes is handed on, to `on_result`, on the worker's thread (a window posts it to its own).
template <typename Request, typename Result> class LatestWins
{
  public:
    using Work = std::function<Result(const Request&, const CancelToken&)>;
    using OnResult = std::function<void(Result)>;

    LatestWins(TaskPool& pool, Work work, OnResult on_result)
        : m_pool{pool}, m_shared{std::make_shared<Shared>(std::move(work), std::move(on_result))}
    {
    }

    // Cancels and waits for what it still runs, so `work` and `on_result` are not called once it is gone.
    ~LatestWins()
    {
        cancel();
        wait();
    }

    LatestWins(const LatestWins&) = delete;
    LatestWins& operator=(const LatestWins&) = delete;

    void request(Request request)
    {
        cancel();
        m_handles.erase(std::remove_if(m_handles.begin(), m_handles.end(),
                                       [](const TaskHandle<void>& handle) { return handle.is_finished(); }),
                        m_handles.end());
        const auto generation{++ m_shared->latest};
        m_handles.push_back(m_pool.submit(
            [shared{m_shared}, generation, request{std::move(request)}](const CancelToken& token)
            {
                auto result{shared->work(request, token)};
                if(token.cancelled() || shared->latest.load() != generation)
                {
                    return;
                }
                shared->on_result(std::move(result));
            }));
    }

    // Cancels every request made so far; none of them is handed on after this returns, but for one already handing.
    void cancel()
    {
        for(auto& handle : m_handles)
        {
            handle.cancel();
        }
    }

    // Waits until every request made has finished or been dropped. Not from `work` or `on_result`.
    void wait()
    {
        for(auto& handle : m_handles)
        {
            handle.wait();
        }
        m_handles.clear();
    }

  private:
    struct Shared
    {
        Shared(Work work_given, OnResult on_result_given)
            : work{std::move(work_given)}, on_result{std::move(on_result_given)}
        {
        }

        Work work;
        OnResult on_result;
        std::atomic<std::uint64_t> latest{0};
    };

    TaskPool& m_pool;
    std::shared_ptr<Shared> m_shared;
    std::vector<TaskHandle<void>> m_handles;
};

namespace Detail
{

// The shares of one concurrent_for. Pool workers and the caller waiting for them each take the next share nobody
// has taken, so the caller finishes the work alone when every worker is busy, and a concurrent_for waited for inside
// a pool task never waits on work nobody can start.
class Shares
{
  public:
    using Run = std::function<void(std::size_t share, const std::atomic<bool>& stopped)>;

    Shares(std::size_t count, Run run) : m_count{count}, m_run{std::move(run)} {}

    [[nodiscard]] std::size_t count() const { return m_count; }

    void run_untaken()
    {
        for(auto share{m_next.fetch_add(1)}; share < m_count; share = m_next.fetch_add(1))
        {
            try
            {
                if(!m_stopped.load())
                {
                    m_run(share, m_stopped);
                }
            }
            catch(...)
            {
                m_stopped.store(true);
                const std::lock_guard<std::mutex> lock{m_mutex};
                if(!m_exception)
                {
                    m_exception = std::current_exception();
                }
            }
            {
                const std::lock_guard<std::mutex> lock{m_mutex};
                ++ m_done;
            }
            m_share_done.notify_all();
        }
    }

    // Runs the shares left, waits for the ones others run, and rethrows the first exception a share threw.
    void finish()
    {
        run_untaken();
        std::unique_lock<std::mutex> lock{m_mutex};
        m_share_done.wait(lock, [this] { return m_done == m_count; });
        if(m_exception)
        {
            std::rethrow_exception(m_exception);
        }
    }

  private:
    const std::size_t m_count;
    const Run m_run;
    std::atomic<std::size_t> m_next{0};
    std::atomic<bool> m_stopped{false};
    std::mutex m_mutex;
    std::condition_variable m_share_done;
    std::size_t m_done{0};
    std::exception_ptr m_exception;
};

// Starts the shares on the pool and gives them back for the caller to finish.
inline std::shared_ptr<Shares> start_shares(TaskPool& pool, std::size_t count, Shares::Run run)
{
    auto shares{std::make_shared<Shares>(count, std::move(run))};
    const auto helpers{std::min<std::size_t>(count, pool.worker_count())};
    for(std::size_t helper{}; helper < helpers; ++ helper)
    {
        // The handle is not kept: the caller waits through the shares, and a dropped helper leaves them to it.
        static_cast<void>(pool.submit([shares](const CancelToken&) { shares->run_untaken(); }));
    }
    return shares;
}

} // namespace Detail

} // namespace olagarro::concurrency
