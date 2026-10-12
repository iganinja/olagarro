#pragma once

#include <oui/application.h>
#include <oui/button.h>
#include <oui/container.h>
#include <oui/label.h>
#include <oui/script/script_engine.h>
#include <oui/script/system_api.h>
#include <oui/text_edit.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// A script the user runs, which is a module that exports one of three functions, or is a tool:
//
//     processFile(file, dryRun)                   works on a document's file, given by its whole path
//     process(dryRun, folder)                     works on its own, from a folder, such as on a whole project's files
//     processGeneratedCode(code, {language, file})
//                                                 takes what an Interactive UI tab's Generate wrote, "javascript"
//                                                 or "cpp", from the description at the whole path `file`; run
//                                                 from here it is for real, and only an Interactive Script tab's
//                                                 run gives it dryRun, true
//     tool, and run(context)                      a tool of the user's: tool says its title, keys and place in the
//                                                 menus, and run works on what the editor has, given in context:
//                                                 {file, language, folder, selection, line, column}
//
// With dryRun true the script changes nothing and says what it would do. What processGeneratedCode gives back is
// text that says how it went. Both get what OUI gives every script
// (oui.readFile, oui.writeFile, oui.listDir, oui.run, oui.confirm...). A run ends when the function returns, or
// when the promise it returns settles. Each run has an engine of its own, so nothing a script leaves behind
// reaches the next.
class ScriptRun
{
  public:
    enum class Kind
    {
        on_file,
        on_its_own,
        on_generated_code,
        tool
    };

    struct Callbacks
    {
        // What console.log prints, and what the programs it runs say.
        std::function<void(std::string_view text)> output;
        // Once the run has ended, always after the call that started it has returned: nothing when it went well,
        // otherwise what went wrong.
        std::function<void(std::optional<std::string> error)> finished;
    };

    ScriptRun(olagarro::oui::Application& application, Callbacks callbacks);
    ~ScriptRun();
    ScriptRun(const ScriptRun&) = delete;
    ScriptRun& operator=(const ScriptRun&) = delete;

    // Loads the module at `script`, which runs what the module does on loading but calls nothing of it: nothing
    // when it is a script that can run, then of kind(), otherwise why it cannot.
    [[nodiscard]] std::optional<std::string> load(const std::filesystem::path& script);
    [[nodiscard]] Kind kind() const;
    // What hears the run end, for when that depends on the kind the script turned out to be.
    void set_finished(std::function<void(std::optional<std::string> error)> finished);
    // Once loaded, runs the function the script exports: processFile(file, dry_run), process(dry_run, folder) or
    // processGeneratedCode(code, {language, file}).
    void run_on_file(const std::filesystem::path& file, bool dry_run);
    void run_on_its_own(const std::filesystem::path& folder, bool dry_run);
    void run_on_generated_code(std::string_view code, std::string_view language, const std::filesystem::path& file);
    // A tool's run(context), `context` an object written in JavaScript.
    void run_tool(std::string_view context);
    // What a tool exports as `tool`: its title, keys and menuPath.
    [[nodiscard]] const olagarro::oui::script::Value& tool() const;
    // Once the run has ended, what its function gave back, as text; nothing when it gave back nothing.
    [[nodiscard]] const std::optional<std::string>& returned() const;
    // Stops the programs the script started; the script goes on with their exit codes.
    void stop_programs();

  private:
    void call(const std::string& function, const std::string& arguments);
    void finish(std::optional<std::string> error);

    olagarro::oui::Application& m_application;
    Callbacks m_callbacks;
    bool m_finished{false};
    // How many parameters each function the module exports declares; none when it exports no such function.
    std::optional<int> m_process_file_parameters;
    std::optional<int> m_process_parameters;
    bool m_process_generated_code{false};
    std::optional<int> m_run_parameters;
    olagarro::oui::script::Value m_tool;
    std::optional<std::string> m_returned;
    std::string m_script_name;
    std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
    // Last, so the work the script started goes before the engine its callbacks run in.
    olagarro::oui::script::ScriptEngine m_engine;
    std::unique_ptr<olagarro::oui::script::SystemApi> m_system;
};

// `text` as a JavaScript string literal.
[[nodiscard]] std::string js_string(std::string_view text);

// What Tools > Script Templates writes in a document to start a script of each kind from.
extern const std::string_view process_file_template;
extern const std::string_view process_template;
extern const std::string_view generated_code_template;
extern const std::string_view tool_template;

// What a script run says, in a tab of its own: what runs on what, its output as it comes, and how it ended,
// with a button to stop the programs it started while it runs.
class ScriptOutput final : public olagarro::oui::Container
{
  public:
    ScriptOutput();

    // Clears it for a new run, titled `title`.
    void begin(std::string_view title);
    void append(std::string_view text);
    // Whether a run is going on, which the stop button follows.
    void set_running(bool running);
    // The output, which takes the focus to go through it; its cursor follows the output to its end.
    [[nodiscard]] olagarro::oui::TextEdit& text_view();

    template <typename Callback> void on_stop(Callback&& callback) { m_stop = std::forward<Callback>(callback); }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    olagarro::oui::Label* m_title{};
    olagarro::oui::Button* m_stop_button{};
    olagarro::oui::TextEdit* m_text{};
    std::function<void()> m_stop;
};

// A folder in the system's temporary one for this OUI Code alone, where dry runs work on their copies: another
// OUI Code running at the same time has its own. It is made the first time it is asked for, and removed as OUI Code
// ends.
[[nodiscard]] const std::filesystem::path& scratch_folder();
