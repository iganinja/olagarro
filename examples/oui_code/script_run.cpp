#include "script_run.h"

#include "ui_descriptions.h"

#include <oui/file_system.h>
#include <oui/grid.h>
#include <oui/translation.h>

#include <chrono>
#include <cstdio>
#include <random>
#include <system_error>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

using oui::script::ScriptError;
using oui::script::SystemApi;
using oui::script::Value;

// What a script run on a file sees as `oui`: what every script reaches, nothing of the editor's own.
constexpr std::string_view prelude{R"js(
{
const system = globalThis.__ouiSystem;
delete globalThis.__ouiSystem;
globalThis.oui = Object.freeze({...system});
}
)js"};

// What the three kinds of script look like, for what tells a script that is none of them.
constexpr std::string_view script_examples{"export function processFile(file, dryRun) {\n"
                                           "    const text = oui.readFile(file);\n"
                                           "    ...\n"
                                           "}\n"
                                           "\n"
                                           "export async function process(dryRun, folder) {\n"
                                           "    for (const name of oui.listDir(folder)) {\n"
                                           "        ...\n"
                                           "    }\n"
                                           "}\n"
                                           "\n"
                                           "export function processGeneratedCode(code, {language, file, dryRun}) "
                                           "{\n"
                                           "    ...\n"
                                           "    return \"How it went.\";\n"
                                           "}\n"
                                           "\n"
                                           "export const tool = {title: \"Count Words\", keys: \"Ctrl+Alt+W\"};\n"
                                           "export async function run({file, selection, line, column}) {\n"
                                           "    ...\n"
                                           "}"};

} // namespace

std::string js_string(std::string_view text)
{
    std::string literal{"\""};
    for(const auto character : text)
    {
        switch(character)
        {
        case '"':
            literal += "\\\"";
            break;
        case '\\':
            literal += "\\\\";
            break;
        case '\n':
            literal += "\\n";
            break;
        case '\r':
            literal += "\\r";
            break;
        default:
            literal += character;
        }
    }
    return literal + "\"";
}

const std::string_view process_file_template{
    R"js(// Runs on the document open in OUI Code. Tools > Run Script picks it; then F5 runs it
// as a dry run and Shift+F5 for real. file is the document's whole path.
// A dry run changes nothing and says what it would do.
export function processFile(file, dryRun) {
    const text = oui.readFile(file);
    const changed = text.replaceAll("\t", "    ");
    if (changed === text) {
        console.log("Nothing to change.");
        return;
    }
    if (dryRun) {
        console.log("Would replace the tabs with spaces.");
        return;
    }
    oui.writeFile(file, changed);
    console.log("Replaced the tabs with spaces.");
}
)js"};

const std::string_view generated_code_template{
    R"js(// Runs after Generate in an Interactive UI tab, chosen beside Generate for the language there. code is what
// Generate wrote, language "javascript" or "cpp", and file the description's whole path. What it gives back
// shows at the end of its output. In an Interactive Script tab it runs as a dry run, which changes nothing and
// says what it would do.
export async function processGeneratedCode(code, {language, file, dryRun}) {
    // Beside the description, with its name: rename_files.cpp for rename_files.oui.
    const target = file.replace(/\.oui$/, language === "cpp" ? ".cpp" : ".js");
    if (oui.exists(target)) {
        return `${target} is there already, and may have been changed since: left as it was.`;
    }
    if (dryRun) {
        return `Would write ${target}, ${code.split("\n").length} lines.`;
    }
    oui.writeFile(target, code);
    // A program, waited for; what it says shows above.
    // const exit = await oui.run("cmake --build build");
    // if (exit !== 0) return "The build failed: see above.";
    return `Wrote ${target}.`;
}
)js"};

const std::string_view process_template{
    R"js(// Runs on its own. Tools > Run Script picks it; then F5 runs it as a dry run and Shift+F5
// for real. folder is the project's folder, or the document's when there is no project.
// A dry run changes nothing and says what it would do.
export async function process(dryRun, folder) {
    const names = oui.listDir(folder).filter((name) => name.endsWith(".txt"));
    for (const name of names) {
        const file = `${folder}/${name}`;
        const text = oui.readFile(file);
        const trimmed = text.split("\n").map((line) => line.trimEnd()).join("\n");
        if (trimmed !== text) {
            console.log(dryRun ? "Would trim" : "Trimming", name);
            if (!dryRun) oui.writeFile(file, trimmed);
        }
    }
    // A program, waited for; what it says shows in the panel below.
    // const code = await oui.run("git status --short", {cwd: folder});
}
)js"};

const std::string_view tool_template{
    R"js(// A tool of your own: saved in the tools folder of your scripts, Tools > User Scripts > Open
// User Script shows where, it gets an entry in the menus, under Tools > User Tools unless menuPath
// says another, and its keys. Tools > User Scripts > Reload User Scripts picks up a new one.
export const tool = {title: "Count Words", keys: "Ctrl+Alt+W"};

// What the editor has: file is the document's whole path, null when it was never saved; selection
// the text selected, "" when none; line and column the cursor's, from 1. The document is saved
// before, so the file says what the editor does. Text it gives back replaces the selection, or goes
// at the cursor; what it prints shows in a tab of its own.
export async function run({file, language, folder, selection, line, column}) {
    const words = selection.split(/\s+/).filter(Boolean).length;
    await oui.message("Count Words", `${words} words selected, at line ${line}.`);
    // A program, waited for; what it says shows in the tab.
    // const code = await oui.run(`wc -w "${file}"`, {cwd: folder});
}
)js"};

ScriptRun::ScriptRun(oui::Application& application, Callbacks callbacks)
    : m_application{application}, m_callbacks{std::move(callbacks)}
{
    // Long enough for real work, short enough that a script stuck in a loop gives the editor back.
    m_engine.set_time_limit(std::chrono::seconds{30});
    m_engine.set_print([this](std::string_view text) { m_callbacks.output(std::string{text} + "\n"); });
    m_engine.set_unhandled_rejection([this](std::string_view error) { m_callbacks.output(std::string{error} + "\n"); });
    m_system = std::make_unique<SystemApi>(
        m_engine, m_application,
        SystemApi::Hooks{[this](std::string_view text, bool) { m_callbacks.output(text); },
                         [this](std::string_view error) { m_callbacks.output(std::string{error} + "\n"); },
                         [] { return true; }, "OUI Code"});
    m_engine.define("__ouiCodeFound",
                    [this](const std::vector<Value>& arguments)
                    {
                        const auto parameters{[&arguments](std::size_t index) -> std::optional<int>
                                              {
                                                  if(index >= arguments.size() || !arguments[index].is_number())
                                                  {
                                                      return std::nullopt;
                                                  }
                                                  return static_cast<int>(arguments[index].to_number());
                                              }};
                        m_process_file_parameters = parameters(0);
                        m_process_parameters = parameters(1);
                        m_process_generated_code = parameters(2).has_value();
                        m_run_parameters = parameters(3);
                        if(arguments.size() > 4)
                        {
                            m_tool = arguments[4];
                        }
                        return Value{};
                    });
    m_engine.define("__ouiCodeFinished",
                    [this](const std::vector<Value>& arguments)
                    {
                        if(arguments.size() > 1 && !arguments[1].is_undefined() && !arguments[1].is_null())
                        {
                            m_returned = arguments[1].to_string();
                        }
                        if(arguments.empty() || arguments.front().is_undefined())
                        {
                            finish(std::nullopt);
                        }
                        else
                        {
                            finish(arguments.front().to_string());
                        }
                        return Value{};
                    });
    m_engine.run(prelude, "oui_code script prelude");
}

ScriptRun::~ScriptRun()
{
    *m_alive = false;
}

std::optional<std::string> ScriptRun::load(const std::filesystem::path& script)
{
    m_script_name = oui::utf8_of(script.filename());
    // A module beside the script, so the script's own imports resolve from its folder; it is never on disk. It
    // keeps the script's exports for the run, and says which functions there are and how many parameters each
    // declares.
    const auto source{"import * as script from " + js_string("./" + m_script_name) +
                      ";\n"
                      "globalThis.__ouiCodeScript = script;\n"
                      "const parameters = (f) => typeof f === \"function\" ? f.length : null;\n"
                      "globalThis.__ouiCodeFound(parameters(script.processFile), parameters(script.process),\n"
                      "    parameters(script.processGeneratedCode), parameters(script.run), script.tool);\n"};
    try
    {
        m_engine.run_module(source, script.parent_path() / oui::path_from_utf8(m_script_name + ".run.js"));
    }
    catch(const ScriptError& error)
    {
        return oui::format_text(oui::translate("ScriptRun", "{0} could not be loaded:\n\n{1}"),
                                {m_script_name, error.what()});
    }
    const auto is_tool{!m_tool.is_undefined()};
    const auto exported{static_cast<int>(m_process_file_parameters.has_value()) +
                        static_cast<int>(m_process_parameters.has_value()) +
                        static_cast<int>(m_process_generated_code) + static_cast<int>(is_tool)};
    if(exported > 1)
    {
        return oui::format_text(oui::translate("ScriptRun", "{0} exports more than one of processFile, process, "
                                                            "processGeneratedCode and tool; a script is one of them."),
                                {m_script_name});
    }
    if(exported == 0)
    {
        return oui::format_text(oui::translate("ScriptRun",
                                               "{0} exports none of processFile(file, dryRun), which runs on the "
                                               "document, process(dryRun, folder), which runs on its own, "
                                               "processGeneratedCode(code, {language, file}), which runs after "
                                               "Generate in an Interactive UI tab, and tool with run(context), a "
                                               "tool of its own in the menus. A script looks like one of these:"),
                                {m_script_name}) +
               "\n\n" + std::string{script_examples};
    }
    if(is_tool && !m_tool.is_object())
    {
        return oui::format_text(oui::translate("ScriptRun", "The tool of {0} must be an object: {title: \"Count "
                                                            "Words\", keys: \"Ctrl+Alt+W\", menuPath: \"tools\"}."),
                                {m_script_name});
    }
    if(is_tool && !m_run_parameters)
    {
        return oui::format_text(oui::translate("ScriptRun", "{0} is a tool, so it must export what it does: "
                                                            "run(context), which gets {file, language, folder, "
                                                            "selection, line, column}."),
                                {m_script_name});
    }
    // dryRun is not optional: a script says what it would do before it does it.
    if(m_process_file_parameters && *m_process_file_parameters < 2)
    {
        return oui::format_text(oui::translate("ScriptRun", "The processFile of {0} must take the file and dryRun: "
                                                            "processFile(file, dryRun)."),
                                {m_script_name});
    }
    if(m_process_parameters && *m_process_parameters < 1)
    {
        return oui::format_text(
            oui::translate("ScriptRun", "The process of {0} must take dryRun: process(dryRun, folder)."),
            {m_script_name});
    }
    return std::nullopt;
}

ScriptRun::Kind ScriptRun::kind() const
{
    return !m_tool.is_undefined()      ? Kind::tool
           : m_process_file_parameters ? Kind::on_file
           : m_process_parameters      ? Kind::on_its_own
                                       : Kind::on_generated_code;
}

void ScriptRun::set_finished(std::function<void(std::optional<std::string> error)> finished)
{
    m_callbacks.finished = std::move(finished);
}

void ScriptRun::run_on_file(const std::filesystem::path& file, bool dry_run)
{
    call("processFile", js_string(oui::utf8_of(file)) + (dry_run ? ", true" : ", false"));
}

void ScriptRun::run_on_its_own(const std::filesystem::path& folder, bool dry_run)
{
    call("process", std::string{dry_run ? "true, " : "false, "} + js_string(oui::utf8_of(folder)));
}

void ScriptRun::run_on_generated_code(std::string_view code, std::string_view language,
                                      const std::filesystem::path& file)
{
    call("processGeneratedCode", js_string(code) + ", {language: " + js_string(language) +
                                     ", file: " + js_string(oui::utf8_of(file)) + ", dryRun: false}");
}

void ScriptRun::run_tool(std::string_view context)
{
    call("run", std::string{context});
}

const oui::script::Value& ScriptRun::tool() const
{
    return m_tool;
}

const std::optional<std::string>& ScriptRun::returned() const
{
    return m_returned;
}

void ScriptRun::call(const std::string& function, const std::string& arguments)
{
    const auto source{
        "{\n"
        "const finished = globalThis.__ouiCodeFinished;\n"
        "Promise.resolve()\n"
        "    .then(() => globalThis.__ouiCodeScript." +
        function + "(" + arguments +
        "))\n"
        "    .then((result) => finished(undefined, result), (error) => finished(error instanceof Error && error.stack\n"
        "        ? `${error}\\n${error.stack}` : String(error)));\n"
        "}\n"};
    try
    {
        m_engine.run(source, m_script_name);
    }
    catch(const ScriptError& error)
    {
        finish(std::string{error.what()});
    }
}

void ScriptRun::stop_programs()
{
    m_system->stop_programs();
}

void ScriptRun::finish(std::optional<std::string> error)
{
    if(m_finished)
    {
        return;
    }
    m_finished = true;
    // Told once the call that ended the run has returned, so whoever hears it may let the run go.
    m_application.post(
        [alive{m_alive}, finished{m_callbacks.finished}, error{std::move(error)}]
        {
            if(*alive)
            {
                finished(error);
            }
        });
}

ScriptOutput::ScriptOutput()
{
    auto ui{build_oui_code_ui(OuiCodeUi::script_output)};
    m_title = &ui.get<oui::Label>("title");
    m_stop_button = &ui.get<oui::Button>("stop");
    m_text = &ui.get<oui::TextEdit>("text");
    static_cast<void>(m_stop_button->on_clicked(
        [this]
        {
            if(m_stop)
            {
                m_stop();
            }
        }));
    add(ui.take());
}

void ScriptOutput::begin(std::string_view title)
{
    m_title->set_text(title);
    m_text->set_text("");
    set_running(true);
}

void ScriptOutput::append(std::string_view text)
{
    m_text->select_range(m_text->text().size(), m_text->text().size());
    m_text->set_read_only(false);
    m_text->paste(text);
    m_text->set_read_only(true);
}

oui::TextEdit& ScriptOutput::text_view()
{
    return *m_text;
}

void ScriptOutput::set_running(bool running)
{
    m_stop_button->set_visible(running);
}

oui::Size ScriptOutput::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({1, 1});
}

void ScriptOutput::arrange(const oui::Rect& bounds)
{
    oui::Widget::arrange(bounds);
    if(child_count() != 0u)
    {
        child_at(0).arrange(bounds);
    }
}

namespace
{

struct ScratchFolder
{
    std::filesystem::path path;

    ~ScratchFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

} // namespace

const std::filesystem::path& scratch_folder()
{
    static const ScratchFolder folder{
        []
        {
            std::random_device random;
            char name[32];
            std::snprintf(name, sizeof name, "oui_code_%08x%08x", random(), random());
            std::error_code error;
            auto path{std::filesystem::temp_directory_path(error) / olagarro::oui::path_from_utf8(name)};
            std::filesystem::create_directories(path, error);
            return path;
        }()};
    return folder.path;
}
