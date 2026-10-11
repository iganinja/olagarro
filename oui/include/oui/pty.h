#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

class Application;

struct PtyOptions
{
    // The program and its arguments, looked for in PATH without a folder: a shell, usually.
    std::vector<std::string> command;
    // Where it runs; OUI's own current folder when empty.
    std::filesystem::path working_directory;
    // The size of the terminal it is told it runs in, in cells.
    int columns{80};
    int rows{24};
};

// What a program in a terminal does, on the application's thread: what it writes, in pieces of any size, and
// how it ended, as Process tells it.
struct PtyCallbacks
{
    std::function<void(std::string_view text)> output;
    std::function<void(int exit_code)> finished;
};

// A program run as in a terminal of its own (a pseudo-terminal): it sees a terminal, with a size, its keys and
// its signals, as a shell needs. Destroying it hangs the terminal up, which ends the program and what it started.
class Pty
{
  public:
    class Impl;
    explicit Pty(std::unique_ptr<Impl> impl);
    ~Pty();
    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    // What is typed, as the terminal sends it: the keys' bytes and escape sequences.
    void write(std::string_view bytes);
    // The terminal's new size, which the program hears of (SIGWINCH).
    void resize(int columns, int rows);
    [[nodiscard]] bool is_running() const;
    // The name of the program that has the terminal now, as the shell or what it started, such as top; empty
    // where the system does not tell.
    [[nodiscard]] std::string foreground_program() const;

  private:
    std::unique_ptr<Impl> m_impl;
};

// Starts `options.command` in a terminal of its own, telling `callbacks` what it does on `application`'s thread.
// Throws std::system_error when it cannot start, as when the program does not exist or the system has no
// terminals to give.
[[nodiscard]] std::unique_ptr<Pty> start_pty(Application& application, PtyOptions options, PtyCallbacks callbacks);

// The user's own shell: SHELL, or the system's when that is not set.
[[nodiscard]] std::vector<std::string> user_shell();

} // namespace olagarro::oui
