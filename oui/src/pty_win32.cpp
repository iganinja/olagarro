#include <oui/pty.h>

#include <oui/application.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace olagarro::oui
{

namespace
{

// The pseudo-console, Windows' terminal for programs since Windows 10 1809. Looked for when first needed, so the
// program still starts on an older Windows, where there is no terminal to give.
using PseudoConsole = void*;
using CreatePseudoConsoleFunction = HRESULT(WINAPI*)(COORD, HANDLE, HANDLE, DWORD, PseudoConsole*);
using ResizePseudoConsoleFunction = HRESULT(WINAPI*)(PseudoConsole, COORD);
using ClosePseudoConsoleFunction = void(WINAPI*)(PseudoConsole);
constexpr DWORD_PTR pseudo_console_attribute{0x00020016};

struct PseudoConsoleApi
{
    CreatePseudoConsoleFunction create{};
    ResizePseudoConsoleFunction resize{};
    ClosePseudoConsoleFunction close{};
};

template <typename Function> [[nodiscard]] Function kernel_function(HMODULE kernel, const char* name)
{
    return reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(kernel, name)));
}

[[nodiscard]] const PseudoConsoleApi& pseudo_console_api()
{
    static const PseudoConsoleApi api{[]
                                      {
                                          PseudoConsoleApi found;
                                          if(const auto kernel{GetModuleHandleW(L"kernel32.dll")})
                                          {
                                              found.create = kernel_function<CreatePseudoConsoleFunction>(
                                                  kernel, "CreatePseudoConsole");
                                              found.resize = kernel_function<ResizePseudoConsoleFunction>(
                                                  kernel, "ResizePseudoConsole");
                                              found.close = kernel_function<ClosePseudoConsoleFunction>(
                                                  kernel, "ClosePseudoConsole");
                                          }
                                          return found;
                                      }()};
    return api;
}

[[nodiscard]] std::wstring to_wide(std::string_view text)
{
    if(text.empty())
    {
        return {};
    }
    const int size{MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0)};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// One argument as CommandLineToArgvW reads it back: quoted when it has spaces or quotes, with the backslashes before
// a quote doubled.
void append_argument(std::wstring& line, const std::wstring& argument)
{
    if(!line.empty())
    {
        line += L' ';
    }
    if(!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        line += argument;
        return;
    }
    line += L'"';
    std::size_t backslashes{};
    for(const auto character : argument)
    {
        if(character == L'\\')
        {
            ++ backslashes;
            continue;
        }
        line.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        line += character;
    }
    line.append(backslashes * 2, L'\\');
    line += L'"';
}

void close_handle(HANDLE& handle)
{
    if(handle != nullptr && handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
    }
    handle = nullptr;
}

[[noreturn]] void throw_last_error(const char* what)
{
    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), what};
}

[[nodiscard]] COORD size_of(int columns, int rows)
{
    return {static_cast<SHORT>(std::clamp(columns, 1, 32767)), static_cast<SHORT>(std::clamp(rows, 1, 32767))};
}

} // namespace

class Pty::Impl
{
  public:
    Impl(Application& application, PtyOptions options, PtyCallbacks callbacks)
        : m_application{application}, m_callbacks{std::make_shared<PtyCallbacks>(std::move(callbacks))}
    {
        if(options.command.empty())
        {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "no command"};
        }
        const auto& api{pseudo_console_api()};
        if(api.create == nullptr || api.resize == nullptr || api.close == nullptr)
        {
            throw std::system_error{std::make_error_code(std::errc::function_not_supported),
                                    "this Windows gives programs no terminal of their own (Windows 10 1809 does)"};
        }
        // What is typed goes in through one pipe and what the program writes comes out through the other, both as
        // UTF-8 with a VT terminal's escape sequences.
        HANDLE console_input{};
        HANDLE console_output{};
        if(!CreatePipe(&console_input, &m_input, nullptr, 0) || !CreatePipe(&m_output, &console_output, nullptr, 0))
        {
            throw_last_error("CreatePipe");
        }
        const auto created{
            api.create(size_of(options.columns, options.rows), console_input, console_output, 0, &m_console)};
        // The pseudo-console keeps copies of its own.
        close_handle(console_input);
        close_handle(console_output);
        if(FAILED(created))
        {
            close_handle(m_input);
            close_handle(m_output);
            throw std::system_error{static_cast<int>(created), std::system_category(), "CreatePseudoConsole"};
        }

        SIZE_T list_size{};
        InitializeProcThreadAttributeList(nullptr, 1, 0, &list_size);
        std::unique_ptr<char[]> list_memory{new char[list_size]};
        auto* const attributes{reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list_memory.get())};
        if(!InitializeProcThreadAttributeList(attributes, 1, 0, &list_size) ||
           !UpdateProcThreadAttribute(attributes, 0, pseudo_console_attribute, m_console, sizeof(m_console), nullptr,
                                      nullptr))
        {
            const auto error{GetLastError()};
            release_console();
            close_handle(m_output);
            throw std::system_error{static_cast<int>(error), std::system_category(), "UpdateProcThreadAttribute"};
        }
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        // Standard handles of none at all, so the program takes the pseudo-console's: otherwise a program run with
        // its output redirected, as under a test runner, passes the redirection on and the terminal sees nothing.
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.lpAttributeList = attributes;
        std::wstring line;
        for(const auto& argument : options.command)
        {
            append_argument(line, to_wide(argument));
        }
        const auto folder{options.working_directory.wstring()};
        // A job holds the program and whatever it starts, so destroying the terminal ends them all.
        m_job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        PROCESS_INFORMATION information{};
        const auto started{CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE,
                                          EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr,
                                          folder.empty() ? nullptr : folder.c_str(), &startup.StartupInfo,
                                          &information)};
        DeleteProcThreadAttributeList(attributes);
        if(!started)
        {
            const auto error{GetLastError()};
            release_console();
            close_handle(m_output);
            close_handle(m_job);
            throw std::system_error{static_cast<int>(error), std::system_category(), options.command.front()};
        }
        AssignProcessToJobObject(m_job, information.hProcess);
        ResumeThread(information.hThread);
        CloseHandle(information.hThread);
        m_process = information.hProcess;
        m_running = true;
        m_reader = std::thread{[this] { read(); }};
        m_waiter = std::thread{[this] { wait(); }};
    }

    ~Impl()
    {
        m_alive->store(false);
        // The program and all it started end, and the waiter then closes the terminal.
        TerminateJobObject(m_job, 1);
        m_waiter.join();
        close_handle(m_job);
        close_handle(m_process);
    }

    void write(std::string_view bytes)
    {
        std::lock_guard<std::mutex> lock{m_input_mutex};
        while(!bytes.empty() && m_input != nullptr)
        {
            DWORD written{};
            if(!WriteFile(m_input, bytes.data(), static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 65536)),
                          &written, nullptr))
            {
                return;
            }
            bytes.remove_prefix(written);
        }
    }

    void resize(int columns, int rows)
    {
        std::lock_guard<std::mutex> lock{m_input_mutex};
        if(m_console != nullptr)
        {
            pseudo_console_api().resize(m_console, size_of(columns, rows));
        }
    }

    [[nodiscard]] bool is_running() const { return m_running.load(); }

  private:
    // Passes on what the program writes, until the terminal is closed.
    void read()
    {
        char buffer[16384];
        DWORD count{};
        while(ReadFile(m_output, buffer, sizeof buffer, &count, nullptr) && count > 0)
        {
            m_application.post(
                [alive{m_alive}, callbacks{m_callbacks}, text{std::string(buffer, count)}]
                {
                    if(alive->load() && callbacks->output)
                    {
                        callbacks->output(text);
                    }
                });
        }
    }

    // Waits for the program to end; then closes the terminal, which ends the reader once it has passed on all the
    // program wrote, and tells how it ended.
    void wait()
    {
        WaitForSingleObject(m_process, INFINITE);
        DWORD exit_code{};
        GetExitCodeProcess(m_process, &exit_code);
        m_running = false;
        {
            std::lock_guard<std::mutex> lock{m_input_mutex};
            release_console();
        }
        m_reader.join();
        close_handle(m_output);
        m_application.post(
            [alive{m_alive}, callbacks{m_callbacks}, code{static_cast<int>(exit_code)}]
            {
                if(alive->load() && callbacks->finished)
                {
                    callbacks->finished(code);
                }
            });
    }

    void release_console()
    {
        if(m_console != nullptr)
        {
            pseudo_console_api().close(m_console);
            m_console = nullptr;
        }
        close_handle(m_input);
    }

    Application& m_application;
    std::shared_ptr<PtyCallbacks> m_callbacks;
    // Cleared when the terminal is destroyed, so news already on its way is dropped.
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    PseudoConsole m_console{};
    // The terminal's input and the pseudo-console itself: the application's thread writes and resizes, the waiter
    // closes them when the program ends.
    std::mutex m_input_mutex;
    HANDLE m_input{};
    HANDLE m_output{};
    HANDLE m_job{};
    HANDLE m_process{};
    std::atomic<bool> m_running{false};
    std::thread m_reader;
    std::thread m_waiter;
};

Pty::Pty(std::unique_ptr<Impl> impl) : m_impl{std::move(impl)}
{
}
Pty::~Pty() = default;

void Pty::write(std::string_view bytes)
{
    m_impl->write(bytes);
}

void Pty::resize(int columns, int rows)
{
    m_impl->resize(columns, rows);
}

bool Pty::is_running() const
{
    return m_impl->is_running();
}

// Windows does not say which program has a console; a program there names the window itself, as cmd does.
std::string Pty::foreground_program() const
{
    return {};
}

std::unique_ptr<Pty> start_pty(Application& application, PtyOptions options, PtyCallbacks callbacks)
{
    return std::make_unique<Pty>(std::make_unique<Pty::Impl>(application, std::move(options), std::move(callbacks)));
}

// The user's command interpreter: COMSPEC, as Windows names it, or cmd.
std::vector<std::string> user_shell()
{
    if(const auto* const shell{std::getenv("COMSPEC")}; shell != nullptr && *shell != '\0')
    {
        return {shell};
    }
    return {"cmd.exe"};
}

} // namespace olagarro::oui
