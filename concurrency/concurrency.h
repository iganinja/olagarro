#pragma once

#include <concurrency/pool.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
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

// A few shares for each thread that can take them, the pool's workers and the caller, so that a share costing more
// than the rest leaves the others something to take; no more than there are elements. Counted in the range's own
// type, as a range longer than an unsigned holds would make it none.
template <typename Size> [[nodiscard]] std::size_t share_count_for(Size range_size, const TaskPool& pool)
{
    constexpr std::size_t shares_per_thread{4};
    const auto wanted{shares_per_thread * (std::size_t{pool.worker_count()} + 1u)};
    return range_size < static_cast<Size>(wanted) ? static_cast<std::size_t>(range_size) : wanted;
}

// Share `share` of `count` over a range of `range_size` elements, as the offsets of its first and its end.
template <typename Size>
[[nodiscard]] std::pair<Size, Size> share_bounds(Size range_size, std::size_t share, std::size_t count)
{
    const auto shares{static_cast<Size>(count)};
    const auto index{static_cast<Size>(share)};
    return {range_size * index / shares, range_size * (index + 1) / shares};
}

} // namespace Detail

// Calls `function(index, element)` for every element of [begin, end) on `pool`'s workers, and on the thread waiting
// for the future, which takes shares too.
template <typename Iterator, typename Function>
[[nodiscard]] std::future<void> concurrent_for(TaskPool& pool, Iterator begin, Iterator end, Function function)
{
    const auto range_size{end - begin};
    if(range_size <= 0)
    {
        return std::async(std::launch::deferred, [] {});
    }

    const auto count{Detail::share_count_for(range_size, pool)};
    auto shared_function{std::make_shared<Function>(std::move(function))};
    auto shares{Detail::start_shares(
        pool, count,
        [begin, range_size, count, shared_function](std::size_t share, const std::atomic<bool>& stopped)
        {
            const auto [first, last]{Detail::share_bounds(range_size, share, count)};
            for(auto offset{first}; offset != last && !stopped.load(); ++ offset)
            {
                (*shared_function)(static_cast<std::size_t>(offset), *(begin + offset));
            }
        })};
    return std::async(std::launch::deferred, [shares] { shares->finish(); });
}

// Folds `transform(index, element)` of every element of [begin, end) with `reduction`, on `pool`'s workers and on
// the thread waiting for the future.
template <typename Iterator, typename Transform, typename Value, typename Combine>
[[nodiscard]] std::future<Value> concurrent_for(TaskPool& pool, Iterator begin, Iterator end, Transform transform,
                                                Reductor<Value, Combine> reduction)
{
    const auto range_size{end - begin};
    if(range_size <= 0)
    {
        return std::async(std::launch::deferred,
                          [reduction{std::move(reduction)}]() mutable { return std::move(reduction.initial_value); });
    }

    const auto count{Detail::share_count_for(range_size, pool)};
    // Each share folds from its first value, so the initial value counts only once, at the end.
    auto partial_results{std::make_shared<std::vector<std::optional<Value>>>(count)};
    auto shared_transform{std::make_shared<Transform>(std::move(transform))};
    auto shared_reduction{std::make_shared<Reductor<Value, Combine>>(std::move(reduction))};
    auto shares{Detail::start_shares(
        pool, count,
        [begin, range_size, count, partial_results, shared_transform,
         shared_reduction](std::size_t share, const std::atomic<bool>& stopped)
        {
            const auto [first, last]{Detail::share_bounds(range_size, share, count)};
            auto& partial{(*partial_results)[share]};
            for(auto offset{first}; offset != last && !stopped.load(); ++ offset)
            {
                auto value{(*shared_transform)(static_cast<std::size_t>(offset), *(begin + offset))};
                if(partial)
                {
                    partial = shared_reduction->combine(std::move(*partial), std::move(value));
                }
                else
                {
                    partial.emplace(std::move(value));
                }
            }
        })};
    return std::async(std::launch::deferred,
                      [shares, partial_results, shared_reduction]
                      {
                          shares->finish();
                          auto& reduction{*shared_reduction};
                          for(auto& value : *partial_results)
                          {
                              if(value)
                              {
                                  reduction.initial_value =
                                      reduction.combine(std::move(reduction.initial_value), std::move(*value));
                              }
                          }
                          return std::move(reduction.initial_value);
                      });
}

// The same on the program's pool, TaskPool::shared().
template <typename Iterator, typename Function>
[[nodiscard]] std::future<void> concurrent_for(Iterator begin, Iterator end, Function function)
{
    return concurrent_for(TaskPool::shared(), begin, end, std::move(function));
}

template <typename Iterator, typename Transform, typename Value, typename Combine>
[[nodiscard]] std::future<Value> concurrent_for(Iterator begin, Iterator end, Transform transform,
                                                Reductor<Value, Combine> reduction)
{
    return concurrent_for(TaskPool::shared(), begin, end, std::move(transform), std::move(reduction));
}

} // namespace olagarro::concurrency
