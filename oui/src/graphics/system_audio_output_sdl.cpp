#include <oui/graphics/system_audio_output.h>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace olagarro::oui
{

namespace
{

constexpr int channels{2};
constexpr int bytes_per_frame{channels * static_cast<int>(sizeof(float))};

// SDL's sound goes, and SDL itself once nothing else of it is open, as a window may still be.
void quit_audio()
{
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if(SDL_WasInit(0) == 0)
    {
        SDL_Quit();
    }
}

class SystemAudioOutput final : public AudioOutput
{
  public:
    explicit SystemAudioOutput(const AudioOutputOptions& options) : m_sample_rate{options.sample_rate}
    {
        if(!options.driver.empty())
        {
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, options.driver.c_str());
        }
        if(!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            throw std::runtime_error{std::string{"Cannot play sound: "} + SDL_GetError()};
        }
        const SDL_AudioSpec spec{SDL_AUDIO_F32, channels, options.sample_rate};
        // SDL converts to what the device takes, if it is not this.
        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if(m_stream == nullptr)
        {
            const std::string error{SDL_GetError()};
            quit_audio();
            throw std::runtime_error{"Cannot play sound: " + error};
        }
        // It opens paused.
        SDL_ResumeAudioStreamDevice(m_stream);
    }

    ~SystemAudioOutput() override
    {
        SDL_DestroyAudioStream(m_stream);
        quit_audio();
    }

    SystemAudioOutput(const SystemAudioOutput&) = delete;
    SystemAudioOutput& operator=(const SystemAudioOutput&) = delete;
    SystemAudioOutput(SystemAudioOutput&&) = delete;
    SystemAudioOutput& operator=(SystemAudioOutput&&) = delete;

    [[nodiscard]] int sample_rate() const override { return m_sample_rate; }

    void queue(const float* samples, std::size_t frames) override
    {
        if(frames == 0)
        {
            return;
        }
        if(SDL_PutAudioStreamData(m_stream, samples, static_cast<int>(frames) * bytes_per_frame))
        {
            m_put += static_cast<std::int64_t>(frames);
        }
    }

    // What was put in, less what the device has not taken yet.
    [[nodiscard]] std::int64_t played() const override { return m_put - queued(); }

    [[nodiscard]] std::int64_t queued() const override
    {
        const auto bytes{SDL_GetAudioStreamQueued(m_stream)};
        return bytes <= 0 ? 0 : bytes / bytes_per_frame;
    }

    void set_paused(bool paused) override
    {
        if(paused)
        {
            SDL_PauseAudioStreamDevice(m_stream);
        }
        else
        {
            SDL_ResumeAudioStreamDevice(m_stream);
        }
    }

    [[nodiscard]] bool is_paused() const override { return SDL_AudioStreamDevicePaused(m_stream); }

    void clear() override
    {
        // Locked, so the device takes nothing between what is counted and what is dropped.
        SDL_LockAudioStream(m_stream);
        m_put -= queued();
        SDL_ClearAudioStream(m_stream);
        SDL_UnlockAudioStream(m_stream);
    }

  private:
    int m_sample_rate;
    SDL_AudioStream* m_stream{};
    // Frames put into the stream since it opened, those cleared taken off.
    std::int64_t m_put{};
};

} // namespace

std::unique_ptr<AudioOutput> system_audio_output(AudioOutputOptions options)
{
    return std::make_unique<SystemAudioOutput>(options);
}

} // namespace olagarro::oui
