#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace olagarro::concurrency
{

// How concurrent_for folds the values its transform makes into one: `combine` takes the result so far and
// the next value. Workers fold their share at once, so `combine` must give the same whatever the grouping, as
// a sum or a maximum do. `initial_value` counts once, at the start of the fold.
template <typename Value, typename Combine> struct Reductor
{
    Value initial_value;
    Combine combine;
};

template <typename Value, typename Combine> [[nodiscard]] auto reductor(Value&& initial_value, Combine&& combine)
{
    return Reductor<std::decay_t<Value>, std::decay_t<Combine>>{std::forward<Value>(initial_value),
                                                                std::forward<Combine>(combine)};
}

namespace Detail
{

// As many workers as the hardware runs at once, and no more than there are elements; counted in the range's own type,
// as a range longer than an unsigned holds would make it none.
template <typename Size> [[nodiscard]] unsigned worker_count_for(Size range_size)
{
    const auto hardware{std::max(1u, std::thread::hardware_concurrency())};
    return range_size < static_cast<Size>(hardware) ? static_cast<unsigned>(range_size) : hardware;
}

inline void join(std::vector<std::thread>& workers)
{
    for(std::thread& worker : workers)
    {
        if(worker.joinable())
        {
            worker.join();
        }
    }
}

template <typename Iterator, typename Function>
void run_concurrent_for(Iterator begin, Iterator end, Function& function)
{
    const auto range_size{end - begin};
    if(range_size <= 0)
    {
        return;
    }

    const auto worker_count{worker_count_for(range_size)};
    std::atomic<bool> cancelled{false};
    std::exception_ptr exception;
    std::mutex exception_mutex;
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    try
    {
        for(auto worker_index{0u}; worker_index < worker_count; ++ worker_index)
        {
            workers.emplace_back(
                [&, worker_index]
                {
                    try
                    {
                        const auto worker_begin{begin + range_size * worker_index / worker_count};
                        const auto worker_end{begin + range_size * (worker_index + 1u) / worker_count};
                        for(auto iterator{worker_begin}; iterator != worker_end && !cancelled.load(); ++ iterator)
                        {
                            function(static_cast<std::size_t>(iterator - begin), *iterator);
                        }
                    }
                    catch(...)
                    {
                        cancelled.store(true);
                        const std::lock_guard<std::mutex> lock{exception_mutex};
                        if(!exception)
                        {
                            exception = std::current_exception();
                        }
                    }
                });
        }
    }
    catch(...)
    {
        join(workers);
        throw;
    }

    join(workers);
    if(exception)
    {
        std::rethrow_exception(exception);
    }
}

template <typename Iterator, typename Transform, typename Value, typename Combine>
Value run_concurrent_for(Iterator begin, Iterator end, Transform& transform, Reductor<Value, Combine>& reduction)
{
    const auto range_size{end - begin};
    if(range_size <= 0)
    {
        return std::move(reduction.initial_value);
    }

    const auto worker_count{worker_count_for(range_size)};
    // Each worker folds its share from its first value, so the initial value counts only once, at the end.
    std::vector<std::optional<Value>> partial_results(worker_count);
    std::atomic<bool> cancelled{false};
    std::exception_ptr exception;
    std::mutex exception_mutex;
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    try
    {
        for(auto worker_index{0u}; worker_index < worker_count; ++ worker_index)
        {
            workers.emplace_back(
                [&, worker_index]
                {
                    try
                    {
                        const auto worker_begin{begin + range_size * worker_index / worker_count};
                        const auto worker_end{begin + range_size * (worker_index + 1u) / worker_count};
                        for(auto iterator{worker_begin}; iterator != worker_end && !cancelled.load(); ++ iterator)
                        {
                            auto& partial{partial_results[worker_index]};
                            auto value{transform(static_cast<std::size_t>(iterator - begin), *iterator)};
                            if(partial)
                            {
                                partial = reduction.combine(std::move(*partial), std::move(value));
                            }
                            else
                            {
                                partial.emplace(std::move(value));
                            }
                        }
                    }
                    catch(...)
                    {
                        cancelled.store(true);
                        const std::lock_guard<std::mutex> lock{exception_mutex};
                        if(!exception)
                        {
                            exception = std::current_exception();
                        }
                    }
                });
        }
    }
    catch(...)
    {
        join(workers);
        throw;
    }

    join(workers);
    if(exception)
    {
        std::rethrow_exception(exception);
    }

    for(auto& value : partial_results)
    {
        if(value)
        {
            reduction.initial_value = reduction.combine(std::move(reduction.initial_value), std::move(*value));
        }
    }
    return std::move(reduction.initial_value);
}

} // namespace Detail

template <typename Iterator, typename Function>
[[nodiscard]] std::future<void> concurrent_for(Iterator begin, Iterator end, Function function)
{
    return std::async(std::launch::async, [begin, end, function{std::move(function)}]() mutable
                      { Detail::run_concurrent_for(begin, end, function); });
}

template <typename Iterator, typename Transform, typename Value, typename Combine>
[[nodiscard]] std::future<Value> concurrent_for(Iterator begin, Iterator end, Transform transform,
                                                Reductor<Value, Combine> reduction)
{
    return std::async(std::launch::async,
                      [begin, end, transform{std::move(transform)}, reduction{std::move(reduction)}]() mutable
                      { return Detail::run_concurrent_for(begin, end, transform, reduction); });
}

} // namespace olagarro::concurrency
