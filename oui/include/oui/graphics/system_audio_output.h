#pragma once

#include <oui/audio_output.h>

#include <memory>

namespace olagarro::oui
{

// Opens the system's default device to play sound on, through SDL3, as the desktop window is opened. The frames
// played are those the device has taken, a few milliseconds of its own buffer ahead of what is heard. It throws when
// no sound can be played, saying why, as on a machine with no sound device or with a driver it does not have.
[[nodiscard]] std::unique_ptr<AudioOutput> system_audio_output(AudioOutputOptions options = {});

} // namespace olagarro::oui
