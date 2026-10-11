#include <oui/pty.h>

#include <oui/application.h>
#include <oui/file_system.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern char** environ;

namespace olagarro::oui
{

namespace
{

[[noreturn]] void fail(const char* what)
{
    throw std::system_error{errno, std::generic_category(), what};
}

// The environment the program gets: OUI's own, telling it the terminal is an xterm of 256 colours, as the
// screen reads its escape sequences.
[[nodiscard]] std::vector<std::string> terminal_environment()
{
    std::vector<std::string> variables;
    for(auto** entry{environ}; *entry != nullptr; ++ entry)
    {
        const std::string_view variable{*entry};
        if(variable.rfind("TERM=", 0) != 0 && variable.rfind("COLORTERM=", 0) != 0 &&
           variable.rfind("COLUMNS=", 0) != 0 && variable.rfind("LINES=", 0) != 0)
        {
            variables.emplace_back(variable);
        }
    }
    variables.emplace_back("TERM=xterm-256color");
    variables.emplace_back("COLORTERM=truecolor");
    return variables;
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
            errno = EINVAL;
            fail("no command");
        }
        m_master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
        if(m_master < 0 || grantpt(m_master) != 0 || unlockpt(m_master) != 0)
        {
            fail("posix_openpt");
        }
        char name[128];
        if(ptsname_r(m_master, name, sizeof name) != 0)
        {
            fail("ptsname");
        }
        resize(options.columns, options.rows);
        if(pipe2(m_wake, O_CLOEXEC | O_NONBLOCK) != 0)
        {
            fail("pipe");
        }

        // A session of its own, whose terminal is the one opened first: the program's input and outputs.
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, name, O_RDWR, 0);
        posix_spawn_file_actions_adddup2(&actions, STDIN_FILENO, STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, STDIN_FILENO, STDERR_FILENO);
        if(!options.working_directory.empty())
        {
            posix_spawn_file_actions_addchdir_np(&actions, options.working_directory.c_str());
        }
        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
        sigset_t none;
        sigemptyset(&none);
        posix_spawnattr_setsigmask(&attributes, &none);
        // The signals OUI handles for itself go back to their defaults, as a fresh program has them.
        sigset_t defaults;
        sigfillset(&defaults);
        sigdelset(&defaults, SIGKILL);
        sigdelset(&defaults, SIGSTOP);
        posix_spawnattr_setsigdefault(&attributes, &defaults);
        std::vector<char*> arguments;
        for(auto& argument : options.command)
        {
            arguments.push_back(argument.data());
        }
        arguments.push_back(nullptr);
        auto variables{terminal_environment()};
        std::vector<char*> environment;
        for(auto& variable : variables)
        {
            environment.push_back(variable.data());
        }
        environment.push_back(nullptr);
        const auto result{
            posix_spawnp(&m_pid, arguments.front(), &actions, &attributes, arguments.data(), environment.data())};
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);
        if(result != 0)
        {
            ::close(m_master);
            ::close(m_wake[0]);
            ::close(m_wake[1]);
            throw std::system_error{result, std::generic_category(), options.command.front()};
        }
        m_running = true;
        m_worker = std::thread{[this] { run(); }};
    }

    ~Impl()
    {
        m_alive->store(false);
        m_stopping = true;
        wake();
        if(m_worker.joinable())
        {
            m_worker.join();
        }
        ::close(m_master);
        ::close(m_wake[0]);
        ::close(m_wake[1]);
    }

    void write(std::string_view bytes)
    {
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            m_outgoing.append(bytes);
        }
        wake();
    }

    void resize(int columns, int rows)
    {
        winsize size{};
        size.ws_col = static_cast<unsigned short>(std::max(1, columns));
        size.ws_row = static_cast<unsigned short>(std::max(1, rows));
        ioctl(m_master, TIOCSWINSZ, &size);
    }

    [[nodiscard]] bool is_running() const { return m_running.load(); }

    [[nodiscard]] std::string foreground_program() const
    {
#ifdef __linux__
        const auto group{::tcgetpgrp(m_master)};
        if(group <= 0)
        {
            return {};
        }
        // Through C's files: C++'s streams would bring their whole machinery of locales into every program
        // with a terminal in it.
        auto name{read_file(path_from_utf8("/proc/" + std::to_string(group) + "/comm")).value_or("")};
        name.erase(std::min(name.size(), name.find('\n')));
        return name;
#else
        return {};
#endif
    }

  private:
    void wake()
    {
        const char byte{1};
        if(::write(m_wake[1], &byte, 1) < 0)
        {
            // A wake-up already pending is enough, so a failed write needs no handling.
        }
    }

    // Reads what the program writes and writes what is typed, until the program ends or the terminal is hung up;
    // then tells how it ended.
    void run()
    {
        sigset_t pipe_signal;
        sigemptyset(&pipe_signal);
        sigaddset(&pipe_signal, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &pipe_signal, nullptr);
        fcntl(m_master, F_SETFL, fcntl(m_master, F_GETFL) | O_NONBLOCK);
        int status{};
        auto reaped{false};
        while(!m_stopping.load())
        {
            bool to_write{};
            {
                std::lock_guard<std::mutex> lock{m_mutex};
                to_write = !m_outgoing.empty();
            }
            pollfd descriptors[2]{{m_master, static_cast<short>(POLLIN | (to_write ? POLLOUT : 0)), 0},
                                  {m_wake[0], POLLIN, 0}};
            // A while at a time, to notice a program that ended while what it started keeps the terminal open.
            if(poll(descriptors, 2, 250) < 0 && errno != EINTR)
            {
                break;
            }
            if((descriptors[1].revents & POLLIN) != 0)
            {
                char drained[64];
                while(::read(m_wake[0], drained, sizeof drained) > 0)
                {
                }
            }
            if((descriptors[0].revents & POLLOUT) != 0)
            {
                std::lock_guard<std::mutex> lock{m_mutex};
                const auto count{::write(m_master, m_outgoing.data(), m_outgoing.size())};
                if(count > 0)
                {
                    m_outgoing.erase(0, static_cast<std::size_t>(count));
                }
            }
            if((descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0)
            {
                char buffer[16384];
                const auto count{::read(m_master, buffer, sizeof buffer)};
                if(count > 0)
                {
                    deliver(std::string(buffer, static_cast<std::size_t>(count)));
                }
                else if(count == 0 || (errno != EAGAIN && errno != EINTR))
                {
                    // Every end of the terminal is closed: the program and all it started are gone.
                    break;
                }
            }
            if(waitpid(m_pid, &status, WNOHANG) == m_pid)
            {
                reaped = true;
                break;
            }
        }
        if(!reaped)
        {
            // Hung up, as a terminal closing: the program gets the signal it would, and a while to end.
            ::kill(-m_pid, SIGHUP);
            for(int tries{}; tries < 20 && waitpid(m_pid, &status, WNOHANG) == 0; ++ tries)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            }
            if(waitpid(m_pid, &status, WNOHANG) == 0)
            {
                ::kill(-m_pid, SIGKILL);
                while(waitpid(m_pid, &status, 0) < 0 && errno == EINTR)
                {
                }
            }
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

    void deliver(std::string text)
    {
        m_application.post(
            [alive{m_alive}, callbacks{m_callbacks}, text{std::move(text)}]
            {
                if(alive->load() && callbacks->output)
                {
                    callbacks->output(text);
                }
            });
    }

    Application& m_application;
    std::shared_ptr<PtyCallbacks> m_callbacks;
    // Cleared when the terminal is destroyed, so news already on its way is dropped.
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
    int m_master{-1};
    int m_wake[2]{-1, -1};
    pid_t m_pid{};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    std::mutex m_mutex;
    std::string m_outgoing;
    std::thread m_worker;
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

std::string Pty::foreground_program() const
{
    return m_impl->foreground_program();
}

std::unique_ptr<Pty> start_pty(Application& application, PtyOptions options, PtyCallbacks callbacks)
{
    return std::make_unique<Pty>(std::make_unique<Pty::Impl>(application, std::move(options), std::move(callbacks)));
}

std::vector<std::string> user_shell()
{
    if(const auto* const shell{std::getenv("SHELL")}; shell != nullptr && *shell != '\0')
    {
        return {shell};
    }
    return {"/bin/sh"};
}

} // namespace olagarro::oui
