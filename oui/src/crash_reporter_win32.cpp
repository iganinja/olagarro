#include <oui/crash_reporter.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>

#include <oui/file_system.h>

#include "crash_cleanup.h"

namespace olagarro::oui
{

namespace
{

CrashReporterOptions installed_options;

// The report path, or the minidump beside it when `extension` is ".dmp".
[[nodiscard]] std::filesystem::path report_path(const wchar_t* const extension)
{
    if(!installed_options.report_path.empty())
    {
        const auto& path{installed_options.report_path};
        return std::wstring{extension} == L"_crash.dmp"
                   ? path.parent_path() / std::filesystem::path{path.stem().native() + L".dmp"}
                   : path;
    }
    std::array<wchar_t, MAX_PATH> path{};
    const auto length{GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))};
    const std::filesystem::path executable{path.data(), path.data() + length};
    return executable.parent_path() / (executable.stem().wstring() + extension);
}

[[nodiscard]] std::string hexadecimal(const std::uint64_t value)
{
    std::array<char, 24> text{};
    std::snprintf(text.data(), text.size(), "0x%llx", static_cast<unsigned long long>(value));
    return text.data();
}

void write_frame(std::string& report, HANDLE process, USHORT index, DWORD64 address)
{
    struct SymbolStorage
    {
        SYMBOL_INFO symbol;
        std::array<char, MAX_SYM_NAME> name{};
    };

    SymbolStorage storage{};
    storage.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    storage.symbol.MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement{};
    report += "#" + std::to_string(index) + " " + hexadecimal(address);
    if(SymFromAddr(process, address, &displacement, &storage.symbol) != FALSE)
    {
        report += " ";
        report += storage.symbol.Name;
    }

    IMAGEHLP_LINE64 source{};
    source.SizeOfStruct = sizeof(source);
    DWORD line_displacement{};
    if(SymGetLineFromAddr64(process, address, &line_displacement, &source) != FALSE)
    {
        report += " (";
        report += source.FileName;
        report += ":" + std::to_string(source.LineNumber) + ")";
    }
    report += '\n';
}

void write_stack_trace(std::string& report, CONTEXT* const exception_context)
{
    constexpr USHORT maximum_frames{64};
    const auto process{GetCurrentProcess()};
    const auto thread{GetCurrentThread()};
    SymInitialize(process, nullptr, TRUE);

    if(exception_context != nullptr)
    {
#if defined(_M_X64) || defined(__x86_64__)
        CONTEXT context{*exception_context};
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        for(USHORT index{}; index < maximum_frames; ++ index)
        {
            write_frame(report, process, index, frame.AddrPC.Offset);
            if(StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                           SymFunctionTableAccess64, SymGetModuleBase64, nullptr) == FALSE)
            {
                break;
            }
        }
#else
        report += "The interrupted stack is unavailable on this architecture.\n";
#endif
    }
    else
    {
        std::array<void*, maximum_frames> frames{};
        const auto count{CaptureStackBackTrace(0, maximum_frames, frames.data(), nullptr)};
        for(USHORT index{}; index < count; ++ index)
        {
            write_frame(report, process, index, reinterpret_cast<DWORD64>(frames.at(index)));
        }
    }
    SymCleanup(process);
}

void write_minidump(EXCEPTION_POINTERS* const exception)
{
    const auto path{report_path(L"_crash.dmp")};
    const auto dump{
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(dump == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION information{};
    information.ThreadId = GetCurrentThreadId();
    information.ExceptionPointers = exception;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump, MiniDumpWithDataSegs,
                      exception == nullptr ? nullptr : &information, nullptr, nullptr);
    CloseHandle(dump);
}

void write_report(const std::string& reason, EXCEPTION_POINTERS* const exception)
{
    try
    {
        std::string report{installed_options.product_name + " crash report\n"};
        report += "Reason: " + reason + "\n\n";
        if(exception != nullptr && exception->ExceptionRecord != nullptr)
        {
            report += "Exception address: " +
                      hexadecimal(reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress)) +
                      "\n";
            report += "Executable base: " + hexadecimal(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))) +
                      "\n\n";
        }
        report += "Stack trace:\n";
        write_stack_trace(report, exception == nullptr ? nullptr : exception->ContextRecord);
        if(auto* const file{open_file(report_path(L"_crash.log"), "wb")})
        {
            std::fwrite(report.data(), 1, report.size(), file);
            std::fclose(file);
        }
        write_minidump(exception);
    }
    catch(...)
    {
    }
}

LONG WINAPI handle_unhandled_exception(EXCEPTION_POINTERS* const exception)
{
    const auto code{exception == nullptr ? 0u : exception->ExceptionRecord->ExceptionCode};
    write_report("Unhandled Windows exception " + hexadecimal(code), exception);
    run_crash_cleanups();
    return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void handle_termination()
{
    std::string reason{"std::terminate"};
    if(const auto exception{std::current_exception()})
    {
        try
        {
            std::rethrow_exception(exception);
        }
        catch(const std::exception& error)
        {
            reason += ": ";
            reason += error.what();
        }
        catch(...)
        {
        }
    }
    write_report(reason, nullptr);
    run_crash_cleanups();
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

} // namespace

void install_crash_reporter(const CrashReporterOptions& options)
{
    installed_options = options;
    SetUnhandledExceptionFilter(handle_unhandled_exception);
    std::set_terminate(handle_termination);
}

} // namespace olagarro::oui
