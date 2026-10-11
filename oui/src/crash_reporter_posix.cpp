#include <oui/crash_reporter.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

// glibc provides backtrace(); musl and some BSDs do not, and there the report goes without a stack trace.
#include "crash_cleanup.h"

#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define OLAGARRO_HAS_BACKTRACE 1
#endif

namespace olagarro::oui
{

namespace
{

// Everything the handler needs is prepared up front: only async-signal-safe calls run after a crash.
std::array<char, 4096> report_path{};
std::array<char, 256> product_name{};

constexpr std::array<int, 5> crash_signals{SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};

void write_text(int descriptor, const char* const text)
{
    // Nothing useful can be done if a write fails while crashing.
    if(write(descriptor, text, std::strlen(text)) < 0)
    {
        return;
    }
}

void handle_crash(int signal_number)
{
#ifdef OLAGARRO_HAS_BACKTRACE
    std::array<void*, 64> frames{};
    const auto frame_count{backtrace(frames.data(), static_cast<int>(frames.size()))};
#endif
    const auto report{open(report_path.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644)};
    if(report >= 0)
    {
        write_text(report, product_name.data());
        write_text(report, " crash report\nUnhandled signal: ");
        write_text(report, strsignal(signal_number));
        write_text(report, "\n");
#ifdef OLAGARRO_HAS_BACKTRACE
        // The binary exports no symbol names, so frames show as offsets; addr2line resolves them against
        // an unstripped build of the same executable.
        write_text(report, "\nStack trace (resolve an offset with: addr2line -Cfe <executable> <offset>):\n");
        backtrace_symbols_fd(frames.data(), frame_count, report);
#endif
        close(report);
    }
    run_crash_cleanups();
    write_text(STDERR_FILENO, "Crashed: ");
    write_text(STDERR_FILENO, strsignal(signal_number));
    write_text(STDERR_FILENO, ". Report written to ");
    write_text(STDERR_FILENO, report_path.data());
    write_text(STDERR_FILENO, "\n");
    std::signal(signal_number, SIG_DFL);
    std::raise(signal_number);
}

[[nodiscard]] std::filesystem::path default_report_path()
{
    std::error_code ec;
    const auto executable{std::filesystem::read_symlink("/proc/self/exe", ec)};
    if(ec)
    {
        return "crash.log";
    }
    return executable.parent_path() / (executable.stem().string() + "_crash.log");
}

} // namespace

void install_crash_reporter(const CrashReporterOptions& options)
{
    const auto path{(options.report_path.empty() ? default_report_path() : options.report_path).string()};
    if(path.size() >= report_path.size())
    {
        return;
    }
    std::copy(path.begin(), path.end(), report_path.begin());
    const auto name{options.product_name.substr(0, product_name.size() - 1)};
    std::copy(name.begin(), name.end(), product_name.begin());
    for(const auto signal_number : crash_signals)
    {
        std::signal(signal_number, handle_crash);
    }
}

} // namespace olagarro::oui
