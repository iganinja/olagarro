#pragma once

#include <filesystem>
#include <string>

namespace olagarro::oui
{

struct CrashReporterOptions
{
    // Names the program at the top of the report.
    std::string product_name{"Application"};
    // Where the report goes; empty: "<executable>_crash.log" beside the executable. On Windows a minidump
    // is written next to it, with the extension ".dmp".
    std::filesystem::path report_path;
};

// Installs process-wide handlers that, when the program crashes, write a report, run the crash cleanups,
// and let the crash go on. Install it once, early.
void install_crash_reporter(const CrashReporterOptions& options = {});

// Registers a function to run after a crash, before the process dies, such as putting a terminal back in
// order. It runs inside a signal handler, so it may only make async-signal-safe calls. Registering the same
// function again has no effect; up to eight are kept.
void add_crash_cleanup(void (*cleanup)());

} // namespace olagarro::oui
