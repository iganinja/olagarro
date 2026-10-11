#include <oui/scripted_backend.h>

namespace olagarro::oui
{

ScriptedBackend::ScriptedBackend(std::vector<Event> events, Size size) : m_events{std::move(events)}, m_size{size}
{
}

void ScriptedBackend::push(const Event& event)
{
    m_events.push_back(event);
    m_drained = false;
}

void ScriptedBackend::set_one_event_per_frame(bool enabled)
{
    m_one_event_per_frame = enabled;
}

void ScriptedBackend::set_on_drained(std::function<void()> callback)
{
    m_on_drained = std::move(callback);
}

void ScriptedBackend::set_keep_frames(bool keep)
{
    m_keep_frames = keep;
}

const std::vector<CellBuffer>& ScriptedBackend::frames() const
{
    return m_frames;
}

const std::optional<CellBuffer>& ScriptedBackend::last_frame() const
{
    return m_last_frame;
}

std::size_t ScriptedBackend::frame_count() const
{
    return m_frame_count;
}

Size ScriptedBackend::size() const
{
    return m_size;
}

std::optional<Event> ScriptedBackend::poll_event()
{
    if(m_next >= m_events.size())
    {
        if(!m_drained && m_on_drained)
        {
            m_drained = true;
            m_on_drained();
        }
        return std::nullopt;
    }
    if(m_one_event_per_frame && (m_frame_count == 0 || m_events_since_frame > 0))
    {
        return std::nullopt;
    }
    ++ m_events_since_frame;
    return m_events.at(m_next++);
}

void ScriptedBackend::present(const CellBuffer& buffer)
{
    ++ m_frame_count;
    m_events_since_frame = 0;
    m_last_frame = buffer;
    if(m_keep_frames)
    {
        m_frames.push_back(buffer);
    }
}

std::unique_ptr<ScriptedBackend> scripted_backend(std::vector<Event> events, Size size)
{
    return std::make_unique<ScriptedBackend>(std::move(events), size);
}

} // namespace olagarro::oui
