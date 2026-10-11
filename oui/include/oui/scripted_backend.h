#pragma once

#include <oui/backend.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace olagarro::oui
{

// A backend without a terminal, for tests and automation. It replays a script of events and keeps what
// it was asked to show.
class ScriptedBackend final : public Backend
{
  public:
    explicit ScriptedBackend(std::vector<Event> events = {}, Size size = {80, 25});

    void push(const Event& event);
    // Hands out at most one event per frame, and only once something has been drawn, the way a person
    // reacts to the screen. Off by default: the script plays as fast as the application polls.
    void set_one_event_per_frame(bool enabled);
    // Runs once, the first time the application asks for an event after the script ran out.
    void set_on_drained(std::function<void()> callback);
    // Keeps every frame instead of only the last one.
    void set_keep_frames(bool keep);
    [[nodiscard]] const std::vector<CellBuffer>& frames() const;
    [[nodiscard]] const std::optional<CellBuffer>& last_frame() const;
    [[nodiscard]] std::size_t frame_count() const;
    // The pointer as the application last set it.
    [[nodiscard]] Pointer pointer() const { return m_pointer; }

    [[nodiscard]] Size size() const override;
    std::optional<Event> poll_event() override;
    void present(const CellBuffer& buffer) override;
    void set_pointer(Pointer pointer) override { m_pointer = pointer; }

  private:
    std::vector<Event> m_events;
    std::size_t m_next{};
    Size m_size;
    bool m_one_event_per_frame{false};
    std::size_t m_events_since_frame{};
    std::function<void()> m_on_drained;
    bool m_drained{false};
    bool m_keep_frames{false};
    std::vector<CellBuffer> m_frames;
    Pointer m_pointer{Pointer::arrow};
    std::optional<CellBuffer> m_last_frame;
    std::size_t m_frame_count{};
};

// A backend that plays `events` instead of reading a terminal, for tests and demonstrations.
[[nodiscard]] std::unique_ptr<ScriptedBackend> scripted_backend(std::vector<Event> events = {}, Size size = {80, 25});

} // namespace olagarro::oui
