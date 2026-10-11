#pragma once

#include <oui/backend.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

// What oui_run runs on before a script opens a window: nothing to show and no input to read; the loop sleeps until
// a process, a timer or a dialog wakes it.
class QuietBackend final : public olagarro::oui::Backend
{
  public:
    [[nodiscard]] olagarro::oui::Size size() const override;
    std::optional<olagarro::oui::Event> poll_event() override;
    void present(const olagarro::oui::CellBuffer& buffer) override;
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override;
    void wake() override;

  private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_woken{false};
};
