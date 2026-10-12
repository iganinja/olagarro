#include "live_script.h"

#include "script_run.h"

#include <oui/file_system.h>
#include <oui/script/script_engine.h>
#include <oui/script/system_api.h>

#include <chrono>
#include <cstdio>
#include <utility>

using namespace olagarro;

namespace
{

using oui::script::ScriptEngine;
using oui::script::ScriptError;
using oui::script::SystemApi;
using oui::script::Value;

constexpr std::chrono::milliseconds time_limit{1000};
// A script that waits on timers is given this long in all before it is told it takes too long.
constexpr std::chrono::milliseconds patience{2000};

// What a live script sees as `oui`: what every script reaches, as when it is run on its own.
constexpr std::string_view prelude{R"js(
{
const system = globalThis.__ouiSystem;
delete globalThis.__ouiSystem;
globalThis.oui = Object.freeze({...system});
}
)js"};

// After the script's own text, as the same module: what it defined that the interactive mode calls, whether it
// exported it or not. On a line of its own, so the script's lines keep their numbers.
constexpr std::string_view footer{"\n;globalThis.__ouiLive = {\n"
                                  "    transform: typeof transform === \"function\" ? transform : undefined,\n"
                                  "    processFile: typeof processFile === \"function\" ? processFile : undefined,\n"
                                  "    process: typeof process === \"function\" ? process : undefined,\n"
                                  "    processGeneratedCode: typeof processGeneratedCode === \"function\"\n"
                                  "        ? processGeneratedCode : undefined};\n"};

// The line of the script an error names, as "<file>:<line>" in its stack.
[[nodiscard]] std::optional<std::size_t> line_of(std::string_view error, const std::string& file)
{
    const auto at{error.find(file + ":")};
    if(at == std::string_view::npos)
    {
        return std::nullopt;
    }
    std::size_t line{};
    auto digits{false};
    for(auto index{at + file.size() + 1}; index < error.size() && error[index] >= '0' && error[index] <= '9'; ++ index)
    {
        line = line * 10 + static_cast<std::size_t>(error[index] - '0');
        digits = true;
    }
    return digits && line > 0 ? std::optional<std::size_t>{line} : std::nullopt;
}

} // namespace

const std::string_view interactive_script_template{
    R"js(// This script runs again as you type, and what it gives shows beside it.
// transform(text) gets the document it runs on and gives back the text in its place: Apply puts it there.
// It only looks: oui.readFile and oui.listDir work, writing files and running programs do not.
// A processFile(file, dryRun) or a process(dryRun, folder) runs here as a dry run too, as Tools > Run Script would.

export function transform(text) {
    const lines = text.split("\n");
    console.log(`${lines.length} lines`);
    return lines.map((line) => line.trimEnd()).join("\n");
}
)js"};

struct LiveScript::Run
{
    // Cleared when the run is let go, so what it would still say goes unheard.
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
    LiveResult result;
    std::string file_name;
    // The copy a processFile works on.
    std::optional<std::filesystem::path> copy;
    bool copy_worked_on{false};
    bool finished{false};
    oui::Timer patience;
    // Before the system, so what the script started goes first.
    ScriptEngine engine;
    std::unique_ptr<SystemApi> system;
};

LiveScript::LiveScript(oui::Application& application, std::function<void(LiveResult)> done)
    : m_application{application}, m_done{std::move(done)}
{
}

LiveScript::~LiveScript()
{
    if(m_run)
    {
        *m_run->alive = false;
    }
}

void LiveScript::run(std::string_view code, const std::filesystem::path& path, std::optional<std::string> input,
                     std::optional<std::filesystem::path> input_file, const std::optional<GeneratedFrom>& generated)
{
    if(m_run)
    {
        *m_run->alive = false;
    }
    m_run = std::make_unique<Run>();
    auto* const run{m_run.get()};
    run->file_name = oui::utf8_of(path.filename());
    // Once, on the application's thread after the call that ended it.
    const auto finish{[this, run](std::optional<std::string> error)
                      {
                          if(run->finished)
                          {
                              return;
                          }
                          run->finished = true;
                          run->patience.stop();
                          if(error)
                          {
                              run->result.error_line = line_of(*error, run->file_name);
                              run->result.error = std::move(error);
                          }
                          else if(run->copy_worked_on)
                          {
                              run->result.text = oui::read_file(*run->copy);
                          }
                          m_application.post(
                              [alive{run->alive}, done{m_done}, result{run->result}]
                              {
                                  if(*alive)
                                  {
                                      done(result);
                                  }
                              });
                      }};
    run->engine.set_time_limit(time_limit);
    run->engine.set_print(
        [run](std::string_view text)
        {
            run->result.log += text;
            run->result.log += '\n';
        });
    run->engine.set_unhandled_rejection([finish](std::string_view error) { finish(std::string{error}); });
    run->engine.define("__ouiLiveDone",
                       [run, finish](const std::vector<Value>& arguments)
                       {
                           if(!arguments.empty() && arguments[0].is_string())
                           {
                               run->result.text = arguments[0].to_string();
                           }
                           run->copy_worked_on = arguments.size() > 1 && arguments[1].to_bool();
                           finish(std::nullopt);
                           return Value{};
                       });
    run->engine.define("__ouiLiveFailed",
                       [finish](const std::vector<Value>& arguments)
                       {
                           finish(arguments.empty() ? std::string{"failed"} : arguments[0].to_string());
                           return Value{};
                       });
    std::string input_literal{"undefined"};
    std::string copy_literal{"undefined"};
    std::string folder_literal{js_string(oui::utf8_of(input_file ? input_file->parent_path() : path.parent_path()))};
    if(input)
    {
        input_literal = js_string(*input);
        // A copy of its own, with the input's name, for a processFile to work on.
        const auto folder{scratch_folder() / oui::path_from_utf8("live")};
        std::error_code ignored;
        std::filesystem::create_directories(folder, ignored);
        run->copy = folder / (input_file ? input_file->filename() : oui::path_from_utf8("input.txt"));
        if(auto* const file{oui::open_file(*run->copy, "wb")})
        {
            std::fwrite(input->data(), 1, input->size(), file);
            std::fclose(file);
        }
        copy_literal = js_string(oui::utf8_of(*run->copy));
    }
    const auto generated_literal{generated ? "{language: " + js_string(generated->language) + ", file: " +
                                                 js_string(oui::utf8_of(generated->file)) + ", dryRun: true}"
                                           : std::string{"{dryRun: true}"}};
    SystemApi::Hooks hooks;
    hooks.read_only = true;
    hooks.print = [run](std::string_view text, bool) { run->result.log += text; };
    hooks.report = [finish](std::string_view error) { finish(std::string{error}); };
    hooks.has_screen = [] { return false; };
    hooks.writable = run->copy;
    run->system = std::make_unique<SystemApi>(run->engine, m_application, hooks);
    const auto call{"(async () => {\n"
                    "    const live = globalThis.__ouiLive;\n"
                    "    const input = " +
                    input_literal +
                    ";\n"
                    "    if(live.transform && input !== undefined) {\n"
                    "        const text = await live.transform(input);\n"
                    "        return [text === undefined || text === null ? undefined : String(text), false];\n"
                    "    }\n"
                    "    if(live.processGeneratedCode && input !== undefined) {\n"
                    "        const text = await live.processGeneratedCode(input, " +
                    generated_literal +
                    ");\n"
                    "        return [text === undefined || text === null ? undefined : String(text), false];\n"
                    "    }\n"
                    "    if(live.processFile && input !== undefined) {\n"
                    "        await live.processFile(" +
                    copy_literal +
                    ", true);\n"
                    "        return [undefined, true];\n"
                    "    }\n"
                    "    if(live.process) {\n"
                    "        await live.process(true, " +
                    folder_literal +
                    ");\n"
                    "    }\n"
                    "    return [undefined, false];\n"
                    "})().then(([text, copied]) => __ouiLiveDone(text, copied), error => __ouiLiveFailed(\n"
                    "    error instanceof Error && error.stack ? `${error}\\n${error.stack}` : String(error)));\n"};
    try
    {
        run->engine.run(prelude, "prelude.js");
        run->engine.run_module(std::string{code} + std::string{footer}, path);
        run->engine.run(call, "interactive.js");
    }
    catch(const ScriptError& error)
    {
        finish(std::string{error.what()});
        return;
    }
    if(!run->finished)
    {
        run->patience =
            m_application.start_timer(patience, [finish] { finish(std::string{"still running after two seconds"}); });
    }
}
