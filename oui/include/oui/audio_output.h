#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace olagarro::oui
{

// Sound played out a block at a time: stereo frames of floats, left and right side by side, from -1 to 1, at a rate
// fixed when it opens. What is queued plays after what was queued before; what it has played is the clock a program
// shows pictures by. Used from one thread at a time.
class AudioOutput
{
  public:
    virtual ~AudioOutput() = default;

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    // Frames a second.
    [[nodiscard]] virtual int sample_rate() const = 0;
    // Queues `frames` frames from `samples`, which holds twice as many floats.
    virtual void queue(const float* samples, std::size_t frames) = 0;
    // The frames played since it opened, those dropped by clear() not counted.
    [[nodiscard]] virtual std::int64_t played() const = 0;
    // The frames queued and not played yet.
    [[nodiscard]] virtual std::int64_t queued() const = 0;
    // Paused, it plays nothing and keeps what is queued; it opens playing.
    virtual void set_paused(bool paused) = 0;
    [[nodiscard]] virtual bool is_paused() const = 0;
    // Drops what is queued and not played yet, as when a program jumps elsewhere.
    virtual void clear() = 0;

  protected:
    AudioOutput() = default;
};

// An output that plays nothing and plays only when told, for tests: a clock that moves when they say.
class SilentAudioOutput final : public AudioOutput
{
  public:
    explicit SilentAudioOutput(int sample_rate = 48000);

    [[nodiscard]] int sample_rate() const override;
    void queue(const float* samples, std::size_t frames) override;
    [[nodiscard]] std::int64_t played() const override;
    [[nodiscard]] std::int64_t queued() const override;
    void set_paused(bool paused) override;
    [[nodiscard]] bool is_paused() const override;
    void clear() override;

    // Plays `frames` frames of what is queued, as a device would in that time: fewer when less is queued, none while
    // paused.
    void advance(std::int64_t frames);

  private:
    int m_sample_rate;
    std::int64_t m_played{};
    std::int64_t m_queued{};
    bool m_paused{false};
};

struct AudioOutputOptions
{
    int sample_rate{48000};
    // The system's way of playing sound, by SDL's name for it, as "pulseaudio", "alsa" or "dummy" (which plays
    // nothing, in time); empty for the one SDL picks. It counts only while no other sound is open.
    std::string driver;
};

} // namespace olagarro::oui
