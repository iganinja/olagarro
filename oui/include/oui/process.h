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

struct ProcessOptions
{
    // The program and its arguments. A program without a folder is looked for in PATH.
    std::vector<std::string> command;
    // Where it runs; OUI's own current folder when empty.
    std::filesystem::path working_directory;
    // Written to its standard input, which is then closed.
    std::string input;
};

// What a process tells while it runs, on the application's thread: its standard output and error as they
// come, in pieces of any size, and how it ended. An exit code above 128 is a process stopped by a signal,
// 128 plus the signal, as shells report it.
struct ProcessCallbacks
{
    std::function<void(std::string_view text)> output;
    std::function<void(std::string_view text)> error;
    std::function<void(int exit_code)> finished;
};

// A program running beside the application. Destroying it stops the program, and whatever the program
// started in turn, and nothing more is told after that.
class Process
{
  public:
    class Impl;
    explicit Process(std::unique_ptr<Impl> impl);
    ~Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    [[nodiscard]] bool is_running() const;
    // Stops the program and what it started; `finished` then tells how it ended.
    void stop();

  private:
    std::unique_ptr<Impl> m_impl;
};

// Starts `options.command`, telling `callbacks` what it does on `application`'s thread. Throws
// std::system_error when the program cannot start, such as when it does not exist.
[[nodiscard]] std::unique_ptr<Process> start_process(Application& application, ProcessOptions options,
                                                     ProcessCallbacks callbacks);

// Hands `target`, a web address or a file, to what the system opens it with, as a browser: xdg-open on POSIX,
// the shell on Windows. It does not wait, and nothing of what opens is tied to the application, which may close
// and leave it. False when the system could not be asked.
bool open_externally(std::string_view target);

// The command that runs `command_line` in the system's shell: sh -c on POSIX, cmd /c on Windows.
[[nodiscard]] std::vector<std::string> shell_command(std::string command_line);

} // namespace olagarro::oui
