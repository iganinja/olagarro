#pragma once

#include <oui/application.h>
#include <oui/script/script_engine.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{
class Process;
} // namespace olagarro::oui

namespace olagarro::oui::script
{

// What scripts reach of the system, the same in every program that runs them: files, programs, timers and
// questions to the user. It defines them on the engine as `__ouiSystem`, an object a program's own prelude
// takes and puts in its `oui` beside what the program adds:
//
//     oui.readFile(path), oui.writeFile(path, text), oui.exists(path), oui.listDir(path)
//     await oui.run(command, {cwd, input, output})   a shell command's exit code; its output goes to
//                                                    output(text) when given; .stop() on the promise stops it
//     await oui.message(title, text), await oui.confirm(question), await oui.prompt(title, label, text),
//     await oui.choose(title, options)               dialogs, or questions on the terminal with no screen
//
// and setTimeout, setInterval, clearTimeout and clearInterval as globals. Paths are UTF-8 text.
class SystemApi
{
  public:
    struct Hooks
    {
        // Where a program's output goes when the script gave no output function; `error` for its errors.
        std::function<void(std::string_view text, bool error)> print;
        // A script's function that threw when called back, as a timer's or a program's.
        std::function<void(std::string_view error)> report;
        // Whether there is a screen to ask on; without one, questions go to the terminal.
        std::function<bool()> has_screen;
        // What titles the dialogs that ask a question.
        std::string title;
        // Only looks: writing files, running programs and asking questions throw instead, as for a script run
        // again and again while it is being written, half typed. It may still write `writable`, when set, as a
        // copy made for it to work on.
        bool read_only{false};
        std::optional<std::filesystem::path> writable{};
    };

    // `engine` and `application` outlive this.
    SystemApi(ScriptEngine& engine, Application& application, Hooks hooks);
    ~SystemApi();
    SystemApi(const SystemApi&) = delete;
    SystemApi& operator=(const SystemApi&) = delete;

    // Programs, timers and questions the scripts started and still wait on.
    [[nodiscard]] int pending() const;
    // Stops every program the scripts started.
    void stop_programs();
    // Calls a script's function, reporting what it throws.
    void call(const Value& function, const std::vector<Value>& arguments = {});

  private:
    using Arguments = std::vector<Value>;

    [[nodiscard]] Value settled(const Value& value);
    template <typename Work> Value later(Work work);
    Value run_program(const Arguments& arguments);
    Value start_timer(const Arguments& arguments);
    void stop_timer(int id);
    Value ask_yes_no(const std::string& question);
    Value ask_text(const std::string& title, const std::string& label, const std::string& text, bool password);
    Value ask_choice(const std::string& title, const Value& options);
    // A file to open or to save as, or a folder, as `mode` says: in a dialog, from `options` as the script gives them.
    Value ask_path(int mode, const Value& options);
    Value tell(const std::string& title, const std::string& text);
    void define_everything();

    ScriptEngine& m_engine;
    Application& m_application;
    Hooks m_hooks;
    std::map<int, std::unique_ptr<Process>> m_programs;
    std::map<int, Timer> m_timers;
    int m_next_id{};
    int m_pending{};
    // Cleared when this goes, so work posted to the application and run after it does nothing.
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
};

} // namespace olagarro::oui::script
