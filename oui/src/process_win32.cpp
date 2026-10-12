#include <oui/process.h>

#include <oui/application.h>

#include <atomic>
#include <system_error>
#include <thread>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>

namespace olagarro::oui
{

namespace
{

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

// One argument as CommandLineToArgvW reads it back: quoted when it has spaces or quotes, with the
// backslashes before a quote doubled.
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

} // namespace

class Process::Impl
{
  public:
    Impl(Application& application, ProcessOptions options, ProcessCallbacks callbacks)
        : m_application{application}, m_callbacks{std::make_shared<ProcessCallbacks>(std::move(callbacks))}
    {
        if(options.command.empty())
        {
            throw std::system_error{std::make_error_code(std::errc::invalid_argument), "no command"};
        }
        SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE input_read{};
        HANDLE input_write{};
        HANDLE output_read{};
        HANDLE output_write{};
        HANDLE error_read{};
        HANDLE error_write{};
        if(!CreatePipe(&input_read, &input_write, &inheritable, 0) ||
           !CreatePipe(&output_read, &output_write, &inheritable, 0) ||
           !CreatePipe(&error_read, &error_write, &inheritable, 0))
        {
            throw_last_error("CreatePipe");
        }
        // Only the child's ends are inherited.
        SetHandleInformation(input_write, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(error_read, HANDLE_FLAG_INHERIT, 0);
        std::wstring line;
        for(std::size_t index{}; index < options.command.size(); ++ index)
        {
            // cmd does not read its command the way programs read arguments: what follows /c goes as it is,
            // in the quotes /s strips.
            if(index > 0 && options.command[index - 1] == "/c" && options.command.front() == "cmd.exe")
            {
                line += L" \"" + to_wide(options.command[index]) + L"\"";
                continue;
            }
            append_argument(line, to_wide(options.command[index]));
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input_read;
        startup.hStdOutput = output_write;
        startup.hStdError = error_write;
        PROCESS_INFORMATION information{};
        const auto folder{options.working_directory.wstring()};
        // A job holds the program and whatever it starts, so stopping it stops them all.
        m_job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        const auto started{CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
                                          CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                                          folder.empty() ? nullptr : folder.c_str(), &startup, &information)};
        close_handle(input_read);
        close_handle(output_write);
        close_handle(error_write);
        if(!started)
        {
            const auto error{GetLastError()};
            close_handle(input_write);
            close_handle(output_read);
            close_handle(error_read);
            close_handle(m_job);
            throw std::system_error{static_cast<int>(error), std::system_category(), options.command.front()};
        }
        AssignProcessToJobObject(m_job, information.hProcess);
        ResumeThread(information.hThread);
        CloseHandle(information.hThread);
        m_process = information.hProcess;
        m_running = true;
        // Anonymous pipes cannot be waited on together, so each has a thread of its own.
        m_writer = std::thread{[handle{input_write}, text{std::move(options.input)}]() mutable
                               {
                                   std::size_t written{};
                                   while(written < text.size())
                                   {
                                       DWORD count{};
                                       if(!WriteFile(handle, text.data() + written,
                                                     static_cast<DWORD>(text.size() - written), &count, nullptr))
                                       {
                                           break;
                                       }
                                       written += count;
                                   }
                                   close_handle(handle);
                               }};
        m_output_reader = std::thread{[this, handle{output_read}]() mutable { read(handle, false); }};
        m_error_reader = std::thread{[this, handle{error_read}]() mutable { read(handle, true); }};
        m_waiter = std::thread{[this] { wait(); }};
    }

    ~Impl()
    {
        m_alive->store(false);
        if(m_running.load())
        {
            TerminateJobObject(m_job, 1);
        }
        // The waiter joins the readers.
        for(auto* const thread : {&m_writer, &m_waiter})
        {
            if(thread->joinable())
            {
                thread->join();
            }
        }
        close_handle(m_process);
        close_handle(m_job);
    }

    [[nodiscard]] bool is_running() const { return m_running.load(); }

    void stop()
    {
        if(m_running.load())
        {
            TerminateJobObject(m_job, 1);
        }
    }

  private:
    void read(HANDLE handle, bool is_error)
    {
        char buffer[16384];
        DWORD count{};
        while(ReadFile(handle, buffer, sizeof(buffer), &count, nullptr) && count > 0)
        {
            m_application.post(
                [alive{m_alive}, callbacks{m_callbacks}, text{std::string(buffer, count)}, is_error]
                {
                    const auto& callback{is_error ? callbacks->error : callbacks->output};
                    if(alive->load() && callback)
                    {
                        callback(text);
                    }
                });
        }
        close_handle(handle);
    }

    // Tells how the program ended once both outputs are read to the end, so nothing it wrote comes after.
    void wait()
    {
        WaitForSingleObject(m_process, INFINITE);
        m_output_reader.join();
        m_error_reader.join();
        DWORD exit_code{};
        GetExitCodeProcess(m_process, &exit_code);
        m_running = false;
        m_application.post(
            [alive{m_alive}, callbacks{m_callbacks}, code{static_cast<int>(exit_code)}]
            {
                if(alive->load() && callbacks->finished)
                {
                    callbacks->finished(code);
                }
            });
    }

    Application& m_application;
    std::shared_ptr<ProcessCallbacks> m_callbacks;
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    HANDLE m_process{};
    HANDLE m_job{};
    std::atomic<bool> m_running{false};
    std::thread m_writer;
    std::thread m_output_reader;
    std::thread m_error_reader;
    std::thread m_waiter;
};

Process::Process(std::unique_ptr<Impl> impl) : m_impl{std::move(impl)}
{
}
Process::~Process() = default;

bool Process::is_running() const
{
    return m_impl->is_running();
}

void Process::stop()
{
    m_impl->stop();
}

std::unique_ptr<Process> start_process(Application& application, ProcessOptions options, ProcessCallbacks callbacks)
{
    return std::make_unique<Process>(
        std::make_unique<Process::Impl>(application, std::move(options), std::move(callbacks)));
}

bool open_externally(std::string_view target)
{
    const auto wide{to_wide(target)};
    // Above 32 when it worked, as ShellExecute has always said it.
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) >
           32;
}

std::vector<std::string> shell_command(std::string command_line)
{
    return {"cmd.exe", "/d", "/s", "/c", std::move(command_line)};
}

void show_typing(bool shown)
{
    auto* const input{GetStdHandle(STD_INPUT_HANDLE)};
    DWORD mode{};
    if(GetConsoleMode(input, &mode) == 0)
    {
        return;
    }
    SetConsoleMode(input, shown ? (mode | ENABLE_ECHO_INPUT) : (mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT)));
}

bool started_from_terminal()
{
    // A console this program shares with another, as the shell it was typed in.
    DWORD processes[2];
    const auto attached{GetConsoleProcessList(processes, 2)};
    if(attached >= 2)
    {
        return true;
    }
    if(attached == 1)
    {
        FreeConsole();
    }
    return false;
}

} // namespace olagarro::oui
