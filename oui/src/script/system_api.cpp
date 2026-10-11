#include <oui/script/system_api.h>

#include <oui/dialogs.h>
#include <oui/file_dialog.h>
#include <oui/file_system.h>
#include <oui/process.h>
#include <oui/quick_pick.h>
#include <oui/translation.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <system_error>
#include <utility>

namespace olagarro::oui::script
{

namespace
{

// The calls scripts see, built on the plain ones defined as `__ouiSystemNative`, which it then hides. What each is,
// for tools/extract_api.js to put in oui/oui.d.ts:
//
// api(system): setTimeout(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number
// Calls fn with args once, ms milliseconds from now; clearTimeout with what it gives stops it.
// api(system): setInterval(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number
// Calls fn with args every ms milliseconds, until clearInterval with what it gives.
// api(system): clearTimeout(id: number): void
// Stops what setTimeout started, if it is still to come.
// api(system): clearInterval(id: number): void
// Stops what setInterval started.
// api(system): oui.run(command: string, options?: {cwd?: string, input?: string, output?: (text: string) => void}): Promise<number> & {stop(): void}
// Runs a shell command and gives its exit code. Its output goes to the terminal, or to output as it comes; input
// is what it reads; cwd the folder it runs in. stop() on the promise stops it. Not in an interactive script.
// api(system): oui.readFile(path: string): string | null
// The file's text, or null when it cannot be read.
// api(system): oui.writeFile(path: string, text: string): boolean
// Writes text as the file, whether it was there or not; false when it cannot. Not in an interactive script.
// api(system): oui.exists(path: string): boolean
// Whether there is a file or a folder at path.
// api(system): oui.listDir(path: string): string[]
// The names in a folder, in order; a folder's ends in "/".
// api(system): oui.message(title: string, text?: string): Promise<void>
// Tells something, in a window when there is one and on the terminal when not. Not in an interactive script.
// api(system): oui.confirm(question: string): Promise<boolean>
// Asks yes or no. Not in an interactive script.
// api(system): oui.prompt(title: string, label?: string, text?: string): Promise<string | null>
// Asks for a line of text, starting with text; null when cancelled. Not in an interactive script.
// api(system): oui.choose(title: string, options: (string | {label: string, detail?: string})[]): Promise<string | null>
// Asks to pick one of options, and gives its label; null when cancelled. Not in an interactive script.
// api(system): oui.openFile(options?: {title?: string, folder?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>
// Asks for a file to open, in the file dialog: from folder, the current one unless given; filters as {name:
// "Scripts", patterns: "*.js;*.ts"}. Gives its path, with / between folders on every system; null when cancelled.
// Without a window, its path is asked on the terminal. Not in an interactive script.
// api(system): oui.saveFile(options?: {title?: string, folder?: string, name?: string, filters?: {name: string, patterns: string}[]}): Promise<string | null>
// As oui.openFile, for a file to save as, name offered: it asks before one that is there is replaced.
// api(system): oui.chooseFolder(options?: {title?: string, folder?: string}): Promise<string | null>
// As oui.openFile, for a folder.
// api(system): oui.copy(text: string): void
// Puts text on the clipboard, the system's where there is one. Not in an interactive script.
// api(system): oui.paste(): string
// What the clipboard holds, as text.
// api(system): oui.openExternally(target: string): boolean
// Hands a web address or a file to what the system opens it with, as a browser, and does not wait for it; false
// when the system could not be asked. Not in an interactive script.
constexpr std::string_view prelude{R"js(
{
const n = globalThis.__ouiSystemNative;
delete globalThis.__ouiSystemNative;

function run(command, options = {}) {
    const [promise, id] = n.run(String(command), options);
    promise.stop = () => n.stop(id);
    return promise;
}

globalThis.setTimeout = (fn, ms = 0, ...rest) => n.timer(Math.max(0, Number(ms) || 0), () => fn(...rest), false);
globalThis.setInterval = (fn, ms = 0, ...rest) => n.timer(Math.max(1, Number(ms) || 0), () => fn(...rest), true);
globalThis.clearTimeout = globalThis.clearInterval = (id) => n.clearTimer(id);

globalThis.__ouiSystem = {
    run,
    readFile: (path) => n.readFile(String(path)),
    writeFile: (path, text) => n.writeFile(String(path), String(text)),
    exists: (path) => n.exists(String(path)),
    listDir: (path) => n.listDir(String(path)),
    message: (title, text = "") => n.message(String(title), String(text)),
    confirm: (question) => n.confirm(String(question)),
    prompt: (title, label = "", text = "") => n.prompt(String(title), String(label), String(text)),
    choose: (title, options) => n.choose(String(title), options),
    copy: (text) => n.copy(String(text)),
    paste: () => n.paste(),
    openExternally: (target) => n.openExternally(String(target)),
    openFile: (options = {}) => n.askPath(0, options),
    saveFile: (options = {}) => n.askPath(2, options),
    chooseFolder: (options = {}) => n.askPath(1, options),
};
}
)js"};

[[nodiscard]] Value argument(const std::vector<Value>& arguments, std::size_t index)
{
    return index < arguments.size() ? arguments[index] : Value{};
}

[[nodiscard]] std::string text_of(const Value& value)
{
    return value.is_undefined() || value.is_null() ? std::string{} : value.to_string();
}

void write_to(std::FILE* stream, std::string_view text)
{
    std::fwrite(text.data(), 1, text.size(), stream);
    std::fflush(stream);
}

// A line typed on the terminal, without its line break; nothing at the end of the input.
[[nodiscard]] std::optional<std::string> read_line()
{
    std::string line;
    char buffer[256];
    while(std::fgets(buffer, sizeof buffer, stdin) != nullptr)
    {
        line += buffer;
        if(!line.empty() && line.back() == '\n')
        {
            line.pop_back();
            if(!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return line;
        }
    }
    return line.empty() ? std::nullopt : std::optional<std::string>{line};
}

} // namespace

SystemApi::SystemApi(ScriptEngine& engine, Application& application, Hooks hooks)
    : m_engine{engine}, m_application{application}, m_hooks{std::move(hooks)}
{
    define_everything();
    m_engine.run(prelude, "oui system prelude");
}

SystemApi::~SystemApi()
{
    *m_alive = false;
}

int SystemApi::pending() const
{
    return m_pending;
}

void SystemApi::stop_programs()
{
    for(auto& [id, program] : m_programs)
    {
        if(program)
        {
            program->stop();
        }
    }
}

void SystemApi::call(const Value& function, const std::vector<Value>& arguments)
{
    try
    {
        static_cast<void>(function.call(arguments));
    }
    catch(const ScriptStop&)
    {
        // Ended by itself, on purpose: nothing to tell.
    }
    catch(const ScriptError& error)
    {
        m_hooks.report(error.what());
    }
}

Value SystemApi::settled(const Value& value)
{
    auto pending{m_engine.promise()};
    static_cast<void>(pending.resolve.call({value}));
    return pending.promise;
}

// A promise settled with what `work` gives once the call that asked has returned, as a dialog must be: it
// waits on the user, and a script must not.
template <typename Work> Value SystemApi::later(Work work)
{
    auto pending{m_engine.promise()};
    ++ m_pending;
    m_application.post(
        [this, alive{m_alive}, pending, work]
        {
            if(!*alive)
            {
                return;
            }
            -- m_pending;
            try
            {
                static_cast<void>(pending.resolve.call({work()}));
            }
            catch(const ScriptStop&)
            {
                // Ended by itself, on purpose: nothing to tell.
            }
            catch(const ScriptError& error)
            {
                m_hooks.report(error.what());
            }
        });
    return pending.promise;
}

Value SystemApi::run_program(const Arguments& arguments)
{
    const auto command{text_of(argument(arguments, 0))};
    const auto options{argument(arguments, 1)};
    ProcessOptions process_options{shell_command(command), {}, {}};
    if(const auto folder{options.get("cwd")}; !folder.is_undefined())
    {
        process_options.working_directory = path_from_utf8(folder.to_string());
    }
    if(const auto input{options.get("input")}; !input.is_undefined())
    {
        process_options.input = input.to_string();
    }
    const auto output{options.get("output")};
    const auto id{++ m_next_id};
    auto pending{m_engine.promise()};
    const auto take_output{[this, output](std::string_view text, bool error)
                           {
                               if(output.is_function())
                               {
                                   call(output, {m_engine.string(text)});
                               }
                               else
                               {
                                   m_hooks.print(text, error);
                               }
                           }};
    ProcessCallbacks callbacks{[take_output](std::string_view text) { take_output(text, false); },
                               [take_output](std::string_view text) { take_output(text, true); },
                               [this, id, pending](int code)
                               {
                                   -- m_pending;
                                   // Gone once this call has returned, not from inside its own callback.
                                   m_application.post(
                                       [this, alive{m_alive}, id]
                                       {
                                           if(*alive)
                                           {
                                               m_programs.erase(id);
                                           }
                                       });
                                   try
                                   {
                                       static_cast<void>(pending.resolve.call({m_engine.number(code)}));
                                   }
                                   catch(const ScriptStop&)
                                   {
                                       // Ended by itself, on purpose: nothing to tell.
                                   }
                                   catch(const ScriptError& error)
                                   {
                                       m_hooks.report(error.what());
                                   }
                               }};
    try
    {
        m_programs[id] = start_process(m_application, std::move(process_options), callbacks);
    }
    catch(const std::system_error& failure)
    {
        throw ScriptError{failure.what()};
    }
    ++ m_pending;
    return m_engine.array({pending.promise, m_engine.number(id)});
}

Value SystemApi::start_timer(const Arguments& arguments)
{
    const auto interval{std::chrono::milliseconds{static_cast<long long>(argument(arguments, 0).to_number())}};
    const auto function{argument(arguments, 1)};
    const auto repeat{argument(arguments, 2).to_bool()};
    const auto id{++ m_next_id};
    ++ m_pending;
    m_timers.emplace(id, m_application.start_timer(interval,
                                                   [this, id, function, repeat]
                                                   {
                                                       if(!repeat)
                                                       {
                                                           stop_timer(id);
                                                       }
                                                       call(function);
                                                   }));
    return m_engine.number(id);
}

void SystemApi::stop_timer(int id)
{
    const auto found{m_timers.find(id)};
    if(found == m_timers.end() || !found->second.is_active())
    {
        return;
    }
    -- m_pending;
    // Stopped now, gone once the call it may be inside has returned.
    found->second.stop();
    m_application.post(
        [this, alive{m_alive}, id]
        {
            if(*alive)
            {
                m_timers.erase(id);
            }
        });
}

Value SystemApi::tell(const std::string& title, const std::string& text)
{
    if(!m_hooks.has_screen())
    {
        write_to(stdout, title + (text.empty() ? "" : ": " + text) + "\n");
        return settled(Value{});
    }
    return later(
        [this, title, text]
        {
            static_cast<void>(message_box(m_application, title, text));
            return Value{};
        });
}

Value SystemApi::ask_yes_no(const std::string& question)
{
    if(!m_hooks.has_screen())
    {
        write_to(stdout, question + " [y/N] ");
        const auto answer{read_line()};
        const auto yes{answer && !answer->empty() &&
                       (std::tolower(static_cast<unsigned char>(answer->front())) == 'y' ||
                        std::tolower(static_cast<unsigned char>(answer->front())) == 's')};
        return settled(m_engine.boolean(yes));
    }
    return later(
        [this, question]
        {
            const auto answer{message_box(m_application, m_hooks.title, question,
                                          {translate("Script", "Yes"), translate("Script", "No")})};
            return m_engine.boolean(answer && *answer == 0);
        });
}

Value SystemApi::ask_text(const std::string& title, const std::string& label, const std::string& text)
{
    if(!m_hooks.has_screen())
    {
        write_to(stdout, (label.empty() ? title : label) + (text.empty() ? "" : " [" + text + "]") + " ");
        const auto answer{read_line()};
        if(!answer)
        {
            return settled(m_engine.null());
        }
        return settled(m_engine.string(answer->empty() ? text : *answer));
    }
    return later(
        [this, title, label, text]
        {
            PromptOptions options;
            options.title = title;
            options.label = label;
            options.text = text;
            const auto answer{prompt(m_application, options)};
            return answer ? m_engine.string(*answer) : m_engine.null();
        });
}

Value SystemApi::ask_path(int mode, const Value& options)
{
    const auto given{[&options](std::string_view property)
                     {
                         if(!options.is_object())
                         {
                             return std::string{};
                         }
                         const auto value{options.get(property)};
                         return value.is_undefined() || value.is_null() ? std::string{} : value.to_string();
                     }};
    FileDialogOptions dialog;
    dialog.mode = static_cast<FileDialogMode>(mode);
    dialog.title = given("title");
    dialog.directory = path_from_utf8(given("folder"));
    dialog.file_name = given("name");
    if(options.is_object())
    {
        const auto filters{options.get("filters")};
        for(std::size_t index{}; filters.is_array() && index < filters.length(); ++ index)
        {
            const auto filter{filters.at(index)};
            dialog.filters.push_back({text_of(filter.get("name")), text_of(filter.get("patterns"))});
        }
    }
    if(!m_hooks.has_screen())
    {
        const auto asked{!dialog.title.empty()                        ? dialog.title
                         : dialog.mode == FileDialogMode::open_folder ? std::string{"Folder"}
                                                                      : std::string{"File"}};
        write_to(stdout, asked + ": ");
        const auto answer{read_line()};
        return settled(answer && !answer->empty() ? m_engine.string(*answer) : m_engine.null());
    }
    return later(
        [this, dialog]
        {
            const auto chosen{file_dialog(m_application, dialog)};
            return chosen ? m_engine.string(utf8_of(*chosen)) : m_engine.null();
        });
}

Value SystemApi::ask_choice(const std::string& title, const Value& options)
{
    if(!options.is_array())
    {
        throw ScriptError{"oui.choose takes a title and a list of options"};
    }
    std::vector<QuickPickItem> items;
    for(std::size_t index{}; index < options.length(); ++ index)
    {
        const auto option{options.at(index)};
        if(const auto label{option.get("label")}; !label.is_undefined())
        {
            items.push_back({label.to_string(), text_of(option.get("detail"))});
        }
        else
        {
            items.push_back({option.to_string(), {}});
        }
    }
    if(!m_hooks.has_screen())
    {
        std::string listing{title + "\n"};
        for(std::size_t index{}; index < items.size(); ++ index)
        {
            listing += "  " + std::to_string(index + 1) + ". " + items[index].label +
                       (items[index].detail.empty() ? "" : "  (" + items[index].detail + ")") + "\n";
        }
        write_to(stdout, listing + "> ");
        const auto answer{read_line()};
        const auto number{answer ? std::atoi(answer->c_str()) : 0};
        if(number < 1 || number > static_cast<int>(items.size()))
        {
            return settled(m_engine.null());
        }
        return settled(m_engine.string(items[static_cast<std::size_t>(number - 1)].label));
    }
    return later(
        [this, title, items]
        {
            QuickPickOptions pick;
            pick.title = title;
            pick.fit_to_items = true;
            const auto chosen{quick_pick(m_application, items, pick)};
            return chosen ? m_engine.string(items[*chosen].label) : m_engine.null();
        });
}

void SystemApi::define_everything()
{
    const auto define{
        [this](std::string_view name, ScriptEngine::Function function)
        {
            if(m_hooks.read_only &&
               (name == "run" || name == "writeFile" || name == "message" || name == "confirm" || name == "prompt" ||
                name == "choose" || name == "askPath" || name == "copy" || name == "openExternally"))
            {
                function = [this, what{std::string{name}}, allowed{std::move(function)}](const Arguments& a) -> Value
                {
                    if(what == "writeFile" && m_hooks.writable &&
                       path_from_utf8(text_of(argument(a, 0))) == *m_hooks.writable)
                    {
                        return allowed(a);
                    }
                    throw ScriptError{"oui." + what + " does nothing while the script runs live; run it on its own"};
                };
            }
            m_engine.define("__ouiSystemNative." + std::string{name}, std::move(function));
        }};
    define("run", [this](const Arguments& a) { return run_program(a); });
    define("stop",
           [this](const Arguments& a)
           {
               const auto id{static_cast<int>(argument(a, 0).to_number())};
               if(const auto found{m_programs.find(id)}; found != m_programs.end() && found->second)
               {
                   found->second->stop();
               }
               return Value{};
           });
    define("timer", [this](const Arguments& a) { return start_timer(a); });
    define("clearTimer",
           [this](const Arguments& a)
           {
               stop_timer(static_cast<int>(argument(a, 0).to_number()));
               return Value{};
           });
    define("message", [this](const Arguments& a) { return tell(text_of(argument(a, 0)), text_of(argument(a, 1))); });
    define("confirm", [this](const Arguments& a) { return ask_yes_no(text_of(argument(a, 0))); });
    define("prompt", [this](const Arguments& a)
           { return ask_text(text_of(argument(a, 0)), text_of(argument(a, 1)), text_of(argument(a, 2))); });
    define("choose", [this](const Arguments& a) { return ask_choice(text_of(argument(a, 0)), argument(a, 1)); });
    define("copy",
           [this](const Arguments& a)
           {
               static_cast<void>(m_application.clipboard().set_text(text_of(argument(a, 0))));
               return Value{};
           });
    define("paste", [this](const Arguments&) { return m_engine.string(m_application.clipboard().text()); });
    define("openExternally",
           [this](const Arguments& a) { return m_engine.boolean(open_externally(text_of(argument(a, 0)))); });
    define("askPath", [this](const Arguments& a)
           { return ask_path(static_cast<int>(argument(a, 0).to_number()), argument(a, 1)); });
    define("readFile",
           [this](const Arguments& a)
           {
               const auto text{read_file(path_from_utf8(text_of(argument(a, 0))))};
               return text ? m_engine.string(*text) : m_engine.null();
           });
    define("writeFile",
           [this](const Arguments& a)
           {
               const auto text{text_of(argument(a, 1))};
               auto* const file{open_file(path_from_utf8(text_of(argument(a, 0))), "wb")};
               if(file == nullptr)
               {
                   return m_engine.boolean(false);
               }
               const auto written{std::fwrite(text.data(), 1, text.size(), file) == text.size()};
               const auto closed{std::fclose(file) == 0};
               return m_engine.boolean(written && closed);
           });
    define("exists",
           [this](const Arguments& a)
           {
               std::error_code ignored;
               return m_engine.boolean(std::filesystem::exists(path_from_utf8(text_of(argument(a, 0))), ignored));
           });
    define("listDir",
           [this](const Arguments& a)
           {
               std::vector<std::string> names;
               std::error_code error;
               for(const auto& entry : list_directory(path_from_utf8(text_of(argument(a, 0))), error))
               {
                   names.push_back(utf8_of(entry.path.filename()) + (entry.is_directory ? "/" : ""));
               }
               std::sort(names.begin(), names.end());
               std::vector<Value> values;
               for(const auto& name : names)
               {
                   values.push_back(m_engine.string(name));
               }
               return m_engine.array(values);
           });
}

} // namespace olagarro::oui::script
