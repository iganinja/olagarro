#include <oui/process.h>

#include <oui/application.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace olagarro::oui
{

namespace
{

// Both ends of a pipe, closed when it goes out of scope unless taken.
struct Pipe
{
    int read{-1};
    int write{-1};
    Pipe()
    {
        int ends[2];
        if(pipe2(ends, O_CLOEXEC) != 0)
        {
            throw std::system_error{errno, std::generic_category(), "pipe"};
        }
        read = ends[0];
        write = ends[1];
    }
    ~Pipe()
    {
        close_end(read);
        close_end(write);
    }
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;

    static void close_end(int& end)
    {
        if(end >= 0)
        {
            ::close(end);
            end = -1;
        }
    }
    [[nodiscard]] int take_read() { return std::exchange(read, -1); }
    [[nodiscard]] int take_write() { return std::exchange(write, -1); }
};

} // namespace

class Process::Impl
{
  public:
    Impl(Application& application, ProcessOptions options, ProcessCallbacks callbacks)
        : m_application{application}, m_callbacks{std::make_shared<ProcessCallbacks>(std::move(callbacks))}
    {
        Pipe input;
        Pipe output;
        Pipe error;
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, input.read, STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, output.write, STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, error.write, STDERR_FILENO);
        if(!options.working_directory.empty())
        {
            posix_spawn_file_actions_addchdir_np(&actions, options.working_directory.c_str());
        }
        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        // A group of its own, so stopping it reaches whatever it starts, as a shell's children.
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);
        std::vector<char*> arguments;
        for(auto& argument : options.command)
        {
            arguments.push_back(argument.data());
        }
        arguments.push_back(nullptr);
        const auto result{options.command.empty() ? EINVAL
                                                  : posix_spawnp(&m_pid, arguments.front(), &actions, &attributes,
                                                                 arguments.data(), environ)};
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);
        if(result != 0)
        {
            throw std::system_error{result, std::generic_category(),
                                    options.command.empty() ? std::string{"no command"} : options.command.front()};
        }
        m_running = true;
        m_worker = std::thread{[this, input_end{input.take_write()}, output_end{output.take_read()},
                                error_end{error.take_read()}, text{std::move(options.input)}]
                               { run(input_end, output_end, error_end, text); }};
    }

    ~Impl()
    {
        m_alive->store(false);
        if(m_running.load())
        {
            ::kill(-m_pid, SIGKILL);
        }
        if(m_worker.joinable())
        {
            m_worker.join();
        }
    }

    [[nodiscard]] bool is_running() const { return m_running.load(); }

    void stop()
    {
        if(m_running.load())
        {
            ::kill(-m_pid, SIGTERM);
        }
    }

  private:
    // Writes the input and reads both outputs together, so a program that writes before it reads cannot
    // block on a full pipe; then waits for the program to end.
    void run(int input_end, int output_end, int error_end, const std::string& text)
    {
        std::size_t written{};
        if(text.empty())
        {
            Pipe::close_end(input_end);
        }
        else
        {
            fcntl(input_end, F_SETFL, fcntl(input_end, F_GETFL) | O_NONBLOCK);
        }
        // A program that stops reading must not stop OUI with SIGPIPE: on this thread the write fails instead.
        sigset_t pipe_signal;
        sigemptyset(&pipe_signal);
        sigaddset(&pipe_signal, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &pipe_signal, nullptr);
        while(output_end >= 0 || error_end >= 0)
        {
            pollfd descriptors[3]{{output_end, POLLIN, 0}, {error_end, POLLIN, 0}, {input_end, POLLOUT, 0}};
            if(poll(descriptors, 3, -1) < 0)
            {
                if(errno == EINTR)
                {
                    continue;
                }
                break;
            }
            if(input_end >= 0 && (descriptors[2].revents & (POLLOUT | POLLERR | POLLHUP)) != 0)
            {
                const auto count{::write(input_end, text.data() + written, text.size() - written)};
                if(count > 0)
                {
                    written += static_cast<std::size_t>(count);
                }
                if(count < 0 || written == text.size())
                {
                    Pipe::close_end(input_end);
                }
            }
            for(auto [index, end, is_error] : {std::tuple{0, &output_end, false}, std::tuple{1, &error_end, true}})
            {
                if(*end < 0 || (descriptors[index].revents & (POLLIN | POLLHUP | POLLERR)) == 0)
                {
                    continue;
                }
                char buffer[16384];
                const auto count{::read(*end, buffer, sizeof(buffer))};
                if(count <= 0)
                {
                    Pipe::close_end(*end);
                    continue;
                }
                deliver(std::string(buffer, static_cast<std::size_t>(count)), is_error);
            }
        }
        Pipe::close_end(input_end);
        int status{};
        while(waitpid(m_pid, &status, 0) < 0 && errno == EINTR)
        {
        }
        m_running = false;
        const auto exit_code{WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)};
        m_application.post(
            [alive{m_alive}, callbacks{m_callbacks}, exit_code]
            {
                if(alive->load() && callbacks->finished)
                {
                    callbacks->finished(exit_code);
                }
            });
    }

    void deliver(std::string text, bool is_error)
    {
        m_application.post(
            [alive{m_alive}, callbacks{m_callbacks}, text{std::move(text)}, is_error]
            {
                const auto& callback{is_error ? callbacks->error : callbacks->output};
                if(alive->load() && callback)
                {
                    callback(text);
                }
            });
    }

    Application& m_application;
    std::shared_ptr<ProcessCallbacks> m_callbacks;
    // Cleared when the process is destroyed, so news already on its way is dropped.
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    pid_t m_pid{};
    std::atomic<bool> m_running{false};
    std::thread m_worker;
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
    // A session of its own, so stopping the application's programs never reaches it, and nothing to write on the
    // terminal the application may be drawing in.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    for(const auto descriptor : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO})
    {
        posix_spawn_file_actions_addopen(&actions, descriptor, "/dev/null",
                                         descriptor == STDIN_FILENO ? O_RDONLY : O_WRONLY, 0);
    }
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID);
    std::string program{"xdg-open"};
    std::string what{target};
    char* arguments[]{program.data(), what.data(), nullptr};
    pid_t child{};
    const auto started{posix_spawnp(&child, program.c_str(), &actions, &attributes, arguments, environ) == 0};
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if(started)
    {
        // Gathered when it ends, which may be after what it opened has started, or never while it stays.
        std::thread{[child]
                    {
                        int status{};
                        static_cast<void>(waitpid(child, &status, 0));
                    }}
            .detach();
    }
    return started;
}

std::vector<std::string> shell_command(std::string command_line)
{
    return {"/bin/sh", "-c", std::move(command_line)};
}

bool started_from_terminal()
{
    // The controlling terminal, which a program started from a terminal has whatever its input and output are.
    const int terminal{open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC)};
    if(terminal < 0)
    {
        return false;
    }
    close(terminal);
    return true;
}

} // namespace olagarro::oui
