#include "quiet_backend.h"

using namespace olagarro;

oui::Size QuietBackend::size() const
{
    return {80, 25};
}

std::optional<oui::Event> QuietBackend::poll_event()
{
    return std::nullopt;
}

void QuietBackend::present(const oui::CellBuffer&)
{
}

void QuietBackend::wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    const auto woken{[this] { return m_woken; }};
    if(deadline)
    {
        static_cast<void>(m_condition.wait_until(lock, *deadline, woken));
    }
    else
    {
        m_condition.wait(lock, woken);
    }
    m_woken = false;
}

void QuietBackend::wake()
{
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        m_woken = true;
    }
    m_condition.notify_one();
}
