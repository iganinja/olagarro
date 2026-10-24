#include <oui/audio_output.h>

#include <algorithm>

namespace olagarro::oui
{

SilentAudioOutput::SilentAudioOutput(int sample_rate) : m_sample_rate{sample_rate}
{
}

int SilentAudioOutput::sample_rate() const
{
    return m_sample_rate;
}

void SilentAudioOutput::queue(const float*, std::size_t frames)
{
    m_queued += static_cast<std::int64_t>(frames);
}

std::int64_t SilentAudioOutput::played() const
{
    return m_played;
}

std::int64_t SilentAudioOutput::queued() const
{
    return m_queued;
}

void SilentAudioOutput::set_paused(bool paused)
{
    m_paused = paused;
}

bool SilentAudioOutput::is_paused() const
{
    return m_paused;
}

void SilentAudioOutput::clear()
{
    m_queued = 0;
}

void SilentAudioOutput::advance(std::int64_t frames)
{
    if(m_paused || frames <= 0)
    {
        return;
    }
    const auto played{std::min(frames, m_queued)};
    m_played += played;
    m_queued -= played;
}

} // namespace olagarro::oui
