#pragma once

namespace olagarro::oui
{

// Runs what add_crash_cleanup registered; safe inside a signal handler.
void run_crash_cleanups();

} // namespace olagarro::oui
