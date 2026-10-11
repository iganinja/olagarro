#pragma once

#include <oui/backend.h>

#include <memory>

namespace olagarro::oui
{

struct TerminalOptions
{
    bool use_alternate_screen{true};
    bool force_legacy{false};
};

class AnsiTerminalBackend final : public Backend
{
  public:
    explicit AnsiTerminalBackend(TerminalOptions options = {});
    ~AnsiTerminalBackend() override;

    AnsiTerminalBackend(const AnsiTerminalBackend&) = delete;
    AnsiTerminalBackend& operator=(const AnsiTerminalBackend&) = delete;

    [[nodiscard]] Size size() const override;
    std::optional<Event> poll_event() override;
    void present(const CellBuffer& buffer) override;
    void reset() override;
    void wait_for_input(std::optional<std::chrono::steady_clock::time_point> deadline) override;
    void wake() override;
    void set_clipboard_text(std::string_view text) override;
    // Prints every raw input record or byte sequence to stderr, to chase keyboard problems.
    void set_diagnose(bool enable);

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// The terminal the program runs in, the backend an application usually wants.
[[nodiscard]] std::unique_ptr<AnsiTerminalBackend> terminal_backend(TerminalOptions options = {});

} // namespace olagarro::oui
