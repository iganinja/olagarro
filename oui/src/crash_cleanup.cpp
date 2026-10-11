#include <oui/crash_reporter.h>

#include <array>
#include <atomic>

#include "crash_cleanup.h"

namespace olagarro::oui
{

namespace
{

// Plain atomics, so a crash handler can read them without taking a lock.
std::array<std::atomic<void (*)()>, 8> cleanups{};

} // namespace

void add_crash_cleanup(void (*const cleanup)())
{
    for(auto& slot : cleanups)
    {
        auto expected{slot.load()};
        if(expected == cleanup)
        {
            return;
        }
        if(expected == nullptr && slot.compare_exchange_strong(expected, cleanup))
        {
            return;
        }
    }
}

void run_crash_cleanups()
{
    for(const auto& slot : cleanups)
    {
        if(const auto cleanup{slot.load()})
        {
            cleanup();
        }
    }
}

} // namespace olagarro::oui
