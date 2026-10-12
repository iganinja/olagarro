#include "workbench.h"

#include "dialogs.h"
#include "interactive_view.h"
#include "live_script.h"
#include "resources.h"
#include "ui_preview.h"
#include "ui_skeleton.h"

#include <oui/oui.h>
#include <oui/terminal_view.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using namespace olagarro;

namespace
{

// Where the settings keep the script that runs after Generate in `language`.
[[nodiscard]] std::string generate_script_setting(SkeletonLanguage language)
{
    return language == SkeletonLanguage::cpp ? "ui_generate_script_cpp" : "ui_generate_script_javascript";
}

} // namespace

std::vector<std::string> Workbench::load_scripts()
{
    auto loaded{std::make_unique<Scripting>(m_options.interface_language)};
    auto problems{loaded->load(built_in_scripts(), m_options.headless
                                                       ? std::nullopt
                                                       : std::optional<std::filesystem::path>{m_scripts_folder})};
    for(const auto& id : m_script_command_ids)
    {
        m_application.commands().unregister_command(id);
    }
    m_script_command_ids.clear();
    m_scripting = std::move(loaded);
    for(const auto& command : m_scripting->commands())
    {
        m_application.commands().register_command(
            command.id, command.title,
            [this, id{command.id}]
            {
                auto* const editor{selected_document_editor(m_editors.active().tabs)};
                const auto& commands{m_scripting->commands()};
                const auto found{std::find_if(commands.begin(), commands.end(),
                                              [&id](const TextCommand& each) { return each.id == id; })};
                if(editor == nullptr || editor->is_read_only() || found == commands.end())
                {
                    return;
                }
                // The selection, or the whole document when there is none.
                const auto cursor{editor->cursor_offset()};
                const auto whole{editor->selected_text().empty()};
                if(whole)
                {
                    editor->select_all();
                }
                try
                {
                    if(const auto result{m_scripting->run(*found, editor->selected_text())})
                    {
                        editor->paste(*result);
                        return;
                    }
                }
                catch(const oui::script::ScriptError& error)
                {
                    show_error(m_application, found->title, error.what());
                }
                if(whole)
                {
                    editor->select_range(cursor, cursor);
                }
            });
        if(command.keys)
        {
            m_application.commands().bind(command.id, *command.keys);
        }
        m_script_command_ids.push_back(command.id);
    }
    auto tool_problems{load_tools()};
    problems.insert(problems.end(), tool_problems.begin(), tool_problems.end());
    for(const auto& tool : m_tools)
    {
        m_application.commands().register_command(tool.id, tool.placement.title, [this, id{tool.id}] { run_tool(id); });
        if(tool.placement.keys)
        {
            m_application.commands().bind(tool.id, *tool.placement.keys);
        }
        m_script_command_ids.push_back(tool.id);
    }
    m_editors.set_languages(&m_scripting->languages());
    m_editors.apply_languages(true);
    return problems;
}

std::vector<std::string> Workbench::load_tools()
{
    m_tools.clear();
    std::vector<std::string> problems;
    std::error_code error;
    const auto folder{m_scripts_folder / oui::path_from_utf8("tools")};
    if(m_options.headless || !std::filesystem::is_directory(folder, error))
    {
        return problems;
    }
    std::vector<std::filesystem::path> files;
    for(const auto& entry : oui::list_directory(folder, error))
    {
        if(!entry.is_directory && oui::utf8_of(entry.path.extension()) == ".js")
        {
            files.push_back(entry.path);
        }
    }
    std::sort(files.begin(), files.end());
    for(const auto& file : files)
    {
        // Loaded for what it says it is; each run loads it again, as it is then.
        ScriptRun reader{m_application, ScriptRun::Callbacks{[](std::string_view) {}, {}}};
        const auto name{"tools/" + oui::utf8_of(file.filename())};
        if(const auto problem{reader.load(file)})
        {
            problems.push_back(*problem);
            continue;
        }
        if(reader.kind() != ScriptRun::Kind::tool)
        {
            problems.push_back(oui::format_text(
                oui::translate("Scripts", "{0} is in the tools folder, but is no tool: a tool exports tool, such as "
                                          "{title: \"Count Words\", keys: \"Ctrl+Alt+W\"}, and run(context)."),
                {name}));
            continue;
        }
        const auto stem{oui::utf8_of(file.stem())};
        try
        {
            m_tools.push_back({"user_tool." + stem,
                               read_placement(reader.tool(), m_options.interface_language, stem, "tool " + name),
                               file});
        }
        catch(const oui::script::ScriptError& failure)
        {
            problems.push_back(name + ": " + failure.what());
        }
    }
    return problems;
}

void Workbench::report_script_problems(const std::vector<std::string>& problems)
{
    if(problems.empty())
    {
        return;
    }
    std::string message;
    for(const auto& problem : problems)
    {
        message += (message.empty() ? "" : "\n\n") + problem;
    }
    show_error(m_application, oui::translate("Scripts", "Scripts"), message);
}

void Workbench::register_tool_commands()
{
    m_application.commands().register_command("tools.reload_scripts", oui::translate("Commands", "Reload User Scripts"),
                                              [this]
                                              {
                                                  report_script_problems(load_scripts());
                                                  fill_menus();
                                              });
    // Alt+X, as Emacs runs a command by its name: the text commands alone, filtered as the palette filters, and the
    // one picked works on the document as from its menu.
    m_application.commands().register_command(
        "tools.run_text_command", oui::translate("Commands", "Run Text Command..."),
        [this]
        {
            std::vector<oui::QuickPickItem> items;
            for(const auto& command : m_scripting->commands())
            {
                items.push_back({command.title, command.keys ? oui::format_shortcut(*command.keys) : std::string{}});
            }
            oui::QuickPickOptions options;
            options.title = oui::translate("Scripts", "Text Commands");
            options.fit_to_items = true;
            options.placement_key = "text_commands";
            if(const auto picked{oui::quick_pick(m_application, items, options)})
            {
                static_cast<void>(m_application.commands().execute(m_scripting->commands().at(*picked).id));
            }
        });
    m_application.commands().bind("tools.run_text_command", {U'x', false, true});
    m_application.commands().register_command(
        "tools.script_output", oui::translate("Commands", "User Scripts Output"),
        [this]
        {
            std::string text;
            for(const auto& line : m_scripting->output())
            {
                text += line + "\n";
            }
            show_markdown(m_application, oui::translate("Scripts", "User Scripts Output"),
                          text.empty() ? oui::translate("Scripts", "No script has printed anything yet.")
                                       : "```\n" + text + "```\n",
                          "script_output");
        });
    m_application.commands().register_command(
        "tools.user_script", oui::translate("Commands", "Open User Script"),
        [this]
        {
            // Made the first time, with examples of what a script can do.
            const auto script{m_scripts_folder / oui::path_from_utf8("user.js")};
            std::error_code error;
            if(!std::filesystem::exists(script, error))
            {
                std::filesystem::create_directories(m_scripts_folder, error);
                auto* const file{oui::open_file(script, "wb")};
                const auto text{user_script_template()};
                const auto written{file != nullptr && std::fwrite(text.data(), 1, text.size(), file) == text.size()};
                if((file != nullptr && std::fclose(file) != 0) || !written)
                {
                    show_error(
                        m_application, oui::translate("Scripts", "Scripts"),
                        oui::format_text(oui::translate("Scripts", "Could not create {0}."), {oui::utf8_of(script)}));
                    return;
                }
            }
            open_from_panel(script);
        });
    m_application.commands().register_command("tools.run_script_choose", oui::translate("Commands", "Run Script..."),
                                              [this] { run_script(true, true); });
    m_application.commands().register_command("tools.run_script_dry",
                                              oui::translate("Commands", "Run Script Again, Dry Run"),
                                              [this] { run_script(true, false); });
    m_application.commands().register_command("tools.run_script",
                                              oui::translate("Commands", "Run Script Again, for Real"),
                                              [this] { run_script(false, false); });
    // Tools > Interactive Script: a script and what it makes, in one tab, run again as it is typed. From a script
    // (.js), it runs on another open document, picked, or on nothing; from any other document, a new script runs on
    // it. It only looks: it writes nothing and runs no programs, so it may run half typed.
    m_application.commands().register_command("tools.interactive_script",
                                              oui::translate("Commands", "&Interactive Script"),
                                              [this] { open_interactive_script(); });
    // Tools > Interactive UI: a user interface description and what it builds, in one tab, built again as it is
    // typed, to try it as it would be used. On the description open, or on a new one from an example.
    m_application.commands().register_command("tools.interactive_ui", oui::translate("Commands", "Interactive &UI"),
                                              [this] { open_interactive_ui(); });
    // An Interactive UI tab's Generate, in each language, from the palette too, for the keyboard: in no menu, as it is
    // the tab's own.
    for(const auto& [id, title, language] :
        {std::tuple{"interactive_ui.generate_javascript",
                    oui::translate("Commands", "Interactive UI: Generate JavaScript"), SkeletonLanguage::javascript},
         std::tuple{"interactive_ui.generate_cpp", oui::translate("Commands", "Interactive UI: Generate C++"),
                    SkeletonLanguage::cpp}})
    {
        m_application.commands().register_command(
            id, title,
            [this, language = language]
            {
                auto& group{m_editors.active()};
                const auto index{group.tabs.selected_index()};
                if(index < group.documents.size() && group.documents[index].interactive_ui)
                {
                    generate_ui_code(*group.documents[index].interactive_ui, language);
                }
            });
    }
    // Tools > New Terminal: the user's shell in a tab, in the open folder; typing exit closes the tab.
    m_application.commands().register_command("tools.terminal", oui::translate("Commands", "New &Terminal"),
                                              [this] { open_terminal(); });
    // The key under Escape, whatever it writes: ` in English, º in Spanish. With Ctrl, and with Alt for a terminal,
    // which has no way to send Ctrl with it: Konsole sends the key alone.
    m_application.commands().bind("tools.terminal", {U'`', true});
    m_application.commands().bind("tools.terminal", {U'º', true});
    m_application.commands().bind("tools.terminal", {U'`', false, true});
    m_application.commands().bind("tools.terminal", {U'º', false, true});
    // Tools > Script Templates: a script of each kind to start from, written where the cursor is.
    m_application.commands().register_command(
        "tools.template_process_file", oui::translate("Commands", "Template: Script on the Document (processFile)"),
        [this] { write_template(process_file_template); });
    m_application.commands().register_command("tools.template_process",
                                              oui::translate("Commands", "Template: Script on Its Own (process)"),
                                              [this] { write_template(process_template); });
    m_application.commands().register_command(
        "tools.template_generated_code",
        oui::translate("Commands", "Template: Script After Generate (processGeneratedCode)"),
        [this] { write_template(generated_code_template); });
    m_application.commands().register_command("tools.template_tool",
                                              oui::translate("Commands", "Template: Tool of Your Own (tool, run)"),
                                              [this] { write_template(tool_template); });
    m_application.commands().bind("tools.run_script_dry", {oui::Key::f5});
    m_application.commands().bind("tools.run_script", {oui::Key::f5, false, false, true});
}

namespace
{

// Text commands to put among a menu's entries: each with the submenus it goes down, from this menu, and its id.
using Placed = std::vector<std::pair<std::vector<std::string>, std::string>>;

// A menu title as a name in menuPath matches it: without the & of its letter, and in lower case.
[[nodiscard]] std::string comparable(std::string_view title)
{
    std::string result;
    for(const auto character : title)
    {
        if(character != '&')
        {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
    }
    return result;
}

// `items` with the commands placed after them: those for this menu as entries, the rest in submenus, joined to one
// of its own whose title they name, in English or as shown, or in one made for them with the name as written.
[[nodiscard]] std::vector<oui::MenuItem> with_placed(std::vector<oui::MenuItem> items, const Placed& placed)
{
    std::vector<oui::MenuItem> added;
    std::vector<std::pair<std::string, Placed>> below;
    for(const auto& entry : placed)
    {
        const auto& path{entry.first};
        if(path.empty())
        {
            added.emplace_back(entry.second);
            continue;
        }
        const auto same{std::find_if(below.begin(), below.end(), [&path](const auto& each)
                                     { return comparable(each.first) == comparable(path.front()); })};
        auto& group{same != below.end() ? same->second : below.emplace_back(path.front(), Placed{}).second};
        group.emplace_back(std::vector<std::string>{path.begin() + 1, path.end()}, entry.second);
    }
    for(const auto& submenu : below)
    {
        const auto& name{submenu.first};
        const auto& group{submenu.second};
        const auto own{std::find_if(items.begin(), items.end(),
                                    [&name](const oui::MenuItem& item)
                                    {
                                        return item.kind() == oui::MenuItem::Kind::submenu &&
                                               (comparable(item.title()) == comparable(name) ||
                                                comparable(oui::untranslate("Menu", item.title())) == comparable(name));
                                    })};
        if(own != items.end())
        {
            *own = oui::menu_submenu(own->title(),
                                     [original{*own}, group{group}] { return with_placed(original.items(), group); });
            continue;
        }
        added.push_back(oui::menu_submenu(name, [group{group}] { return with_placed({}, group); }));
    }
    if(!added.empty() && !items.empty())
    {
        items.push_back(oui::menu_separator());
    }
    items.insert(items.end(), added.begin(), added.end());
    return items;
}

} // namespace

std::vector<oui::MenuItem> Workbench::with_text_commands(std::string_view menu, std::vector<oui::MenuItem> items)
{
    Placed placed;
    const auto place{[&placed, menu](const std::vector<std::string>& path, const std::string& id)
                     {
                         if(!path.empty() && path.front() == menu)
                         {
                             placed.emplace_back(std::vector<std::string>{path.begin() + 1, path.end()}, id);
                         }
                     }};
    for(const auto& command : m_scripting->commands())
    {
        place(command.menu_path, command.id);
    }
    for(const auto& tool : m_tools)
    {
        place(tool.placement.menu_path, tool.id);
    }
    return with_placed(std::move(items), placed);
}

std::vector<oui::MenuItem> Workbench::tools_menu()
{
    // The scripts OUI Code loads, which define languages and text commands, not those run from here.
    const auto user_scripts{
        [] { return std::vector<oui::MenuItem>{"tools.user_script", "tools.reload_scripts", "tools.script_output"}; }};
    return {"edit.filter",
            oui::menu_submenu(oui::translate("Menu", "Te&xt Commands"),
                              [this]
                              {
                                  std::vector<oui::MenuItem> items;
                                  // Those that say no other place in the menus.
                                  for(const auto& command : m_scripting->commands())
                                  {
                                      if(command.menu_path.empty())
                                      {
                                          items.emplace_back(command.id);
                                      }
                                  }
                                  if(items.empty())
                                  {
                                      items.push_back(oui::menu_action(oui::translate("Menu", "(none)"), [] {}, false));
                                  }
                                  return items;
                              }),
            "tools.run_text_command",
            oui::menu_submenu(oui::translate("Menu", "User T&ools"),
                              [this]
                              {
                                  std::vector<oui::MenuItem> items;
                                  // Those that say no other place in the menus.
                                  for(const auto& tool : m_tools)
                                  {
                                      if(tool.placement.menu_path.empty())
                                      {
                                          items.emplace_back(tool.id);
                                      }
                                  }
                                  if(items.empty())
                                  {
                                      items.push_back(oui::menu_action(oui::translate("Menu", "(none)"), [] {}, false));
                                  }
                                  return items;
                              }),
            oui::menu_separator(),
            "tools.run_script_choose",
            "tools.run_script_dry",
            "tools.run_script",
            oui::menu_submenu(oui::translate("Menu", "Script Te&mplates"),
                              []
                              {
                                  return std::vector<oui::MenuItem>{
                                      "tools.template_process_file", "tools.template_process",
                                      "tools.template_generated_code", "tools.template_tool"};
                              }),
            oui::menu_separator(),
            oui::menu_submenu(oui::translate("Menu", "User Scri&pts"), user_scripts),
            oui::menu_separator(),
            "tools.interactive_script",
            "tools.interactive_ui",
            "tools.terminal",
            oui::menu_separator(),
            "tools.compare_files",
            "tools.compare_saved",
            "tools.calculator"};
}

void Workbench::open_interactive_script()
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    auto* const current{selected_document_editor(group.tabs)};
    const auto title{oui::translate("Commands", "Interactive Script")};
    if(current == nullptr || index >= group.documents.size() || group.documents[index].is_interactive())
    {
        show_error(m_application, title,
                   oui::translate("ScriptRun", "Open the script first, or the document for a script to work on."));
        return;
    }
    const auto& document{group.documents[index]};
    InteractiveStart start;
    if(document.path && document.path->extension() == ".js")
    {
        start.script_file = document.path;
        start.code = current->text();
        // What it runs on: text pasted in the tab, another document open, or nothing, for a script that only prints.
        std::vector<oui::QuickPickItem> items{{oui::translate("ScriptRun", "(Nothing: what it prints only)"), {}},
                                              {oui::translate("ScriptRun", "(Text pasted in the tab)"), {}}};
        std::vector<std::pair<oui::TextEdit*, std::optional<std::filesystem::path>>> inputs{{nullptr, std::nullopt},
                                                                                            {nullptr, std::nullopt}};
        for(auto& each : m_editors.groups())
        {
            for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
            {
                auto* const editor{document_editor(each.tabs, tab)};
                if(editor == nullptr || editor == current || each.documents[tab].interactive)
                {
                    continue;
                }
                const auto& file{each.documents[tab].path};
                items.push_back({file ? title_for(*file) : oui::translate("Files", "Untitled"),
                                 file ? shown_path(*file) : std::string{}});
                inputs.emplace_back(editor, file);
            }
        }
        oui::QuickPickOptions options;
        options.title = title;
        options.placeholder = oui::translate("ScriptRun", "What the script runs on");
        options.fit_to_items = true;
        const auto picked{oui::quick_pick(m_application, items, options)};
        if(!picked)
        {
            return;
        }
        if(*picked == 1)
        {
            start.pasted = std::string{};
        }
        start.input = inputs[*picked].first;
        start.input_file = inputs[*picked].second;
        if(start.input != nullptr)
        {
            start.input_name = items[*picked].label;
        }
    }
    else
    {
        start.code = interactive_script_template;
        start.input = current;
        start.input_file = document.path;
        start.input_name = document.path ? title_for(*document.path) : oui::translate("Files", "Untitled");
    }
    start_interactive_script(std::move(start));
}

void Workbench::start_interactive_script(InteractiveStart start)
{
    auto& group{m_editors.active()};
    const auto title{oui::translate("Commands", "Interactive Script")};
    const auto& script_file{start.script_file};
    auto session{std::make_shared<InteractiveSession>()};
    session->input = start.input;
    session->input_file = start.input_file;
    session->input_name = start.input_name;
    session->generated = start.generated;
    const auto folder{session->input_file  ? session->input_file->parent_path()
                      : session->generated ? session->generated->file.parent_path()
                      : m_project_folder   ? *m_project_folder
                                           : std::filesystem::current_path()};
    session->module_path = script_file ? *script_file : folder / oui::path_from_utf8("interactive.js");
    auto editor{make_editor()};
    apply_tab_settings(*editor, m_settings);
    editor->set_text(start.code);
    auto view{std::make_unique<InteractiveView>(std::move(editor))};
    session->view = view.get();
    auto* const raw{session.get()};
    if(start.pasted)
    {
        session->view->show_input(*start.pasted);
        session->input = &session->view->input();
        session->input_name = oui::translate("Interactive", "Input");
    }
    // What a processGeneratedCode gives back says how it would go, not text to put anywhere.
    session->view->set_applies(!session->generated);
    session->live = std::make_unique<LiveScript>(m_application, [raw](LiveResult result)
                                                 { raw->view->show(result, raw->input_name); });
    // The input may have been closed since: then the script runs on nothing.
    const auto run{[raw]
                   {
                       auto* const input{raw->input.get()};
                       if(input == nullptr)
                       {
                           raw->input_name.clear();
                       }
                       raw->live->run(raw->view->code().text(), raw->module_path,
                                      input != nullptr ? std::optional<std::string>{input->text()} : std::nullopt,
                                      raw->input_file, raw->generated);
                   }};
    // Again a moment after the last change, not at every key.
    const auto soon{[this, raw, run]
                    {
                        raw->pending = m_application.start_timer(std::chrono::milliseconds{300},
                                                                 [raw, run]
                                                                 {
                                                                     raw->pending.stop();
                                                                     run();
                                                                 });
                    }};
    session->code_changed = signal_slot::ScopedConnection{session->view->code().on_changed(soon)};
    if(session->input)
    {
        session->input_changed = signal_slot::ScopedConnection{session->input->on_changed(soon)};
    }
    session->view->on_apply(
        [this, raw]
        {
            if(raw->input.refers_to(&raw->view->input()) && raw->view->result())
            {
                raw->view->input().select_all();
                raw->view->input().paste(*raw->view->result());
                return;
            }
            for(auto& each : m_editors.groups())
            {
                for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
                {
                    auto* const editor{document_editor(each.tabs, tab)};
                    if(raw->input.refers_to(editor) && raw->view->result() && !editor->is_read_only())
                    {
                        editor->select_all();
                        editor->paste(*raw->view->result());
                        return;
                    }
                }
            }
        });
    session->view->on_error_chosen(
        [this, raw](std::size_t line)
        {
            static_cast<void>(raw->view->code().go_to_line(line));
            m_application.focus(raw->view->code());
        });
    auto* const code_editor{&session->view->code()};
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.emplace_back();
    group.documents.back().path = script_file;
    group.documents.back().chosen_language = std::string{"javascript"};
    group.documents.back().interactive = session;
    group.tabs.add_tab(script_file ? document_tab_title(*script_file, true) : title, std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    m_editors.apply_languages(true);
    m_application.focus(*code_editor);
    run();
}

void Workbench::open_interactive_ui()
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    auto* const current{selected_document_editor(group.tabs)};
    // Asked again from an Interactive UI tab, it stays there.
    if(current != nullptr && index < group.documents.size() && group.documents[index].interactive_ui)
    {
        m_application.focus(*current);
        return;
    }
    // On the description open, as a second editor on its file: the one tab there is for it, where it is, or a new
    // one; from anything else, a new one from an example.
    if(current != nullptr && index < group.documents.size() && !group.documents[index].is_interactive() &&
       group.documents[index].path && group.documents[index].path->extension() == ".oui")
    {
        const auto file{*group.documents[index].path};
        for(auto& each : m_editors.groups())
        {
            for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
            {
                if(each.documents[tab].interactive_ui && each.documents[tab].path == file)
                {
                    each.tabs.select(tab);
                    focus_current_tab(m_application, each.tabs);
                    return;
                }
            }
        }
        start_interactive_ui(file, current->text());
        return;
    }
    start_interactive_ui(std::nullopt, interactive_ui_template);
}

void Workbench::start_interactive_ui(std::optional<std::filesystem::path> file, std::string_view text)
{
    auto& group{m_editors.active()};
    auto session{std::make_shared<InteractiveUiSession>()};
    auto editor{make_editor()};
    apply_tab_settings(*editor, m_settings);
    editor->set_text(text);
    auto view{std::make_unique<UiPreviewView>(std::move(editor))};
    session->view = view.get();
    auto* const raw{session.get()};
    // Again a moment after the last change, not at every key.
    session->code_changed = signal_slot::ScopedConnection{session->view->code().on_changed(
        [this, raw]
        {
            raw->pending = m_application.start_timer(std::chrono::milliseconds{300},
                                                     [raw]
                                                     {
                                                         raw->pending.stop();
                                                         raw->view->show(raw->view->code().text());
                                                     });
        })};
    session->view->on_error_chosen(
        [this, raw](std::size_t line)
        {
            static_cast<void>(raw->view->code().go_to_line(line));
            m_application.focus(raw->view->code());
        });
    // In the language chosen last, remembered from one tab and one start to the next.
    session->view->set_language(m_settings.value("ui_generate_language", "javascript") == "cpp"
                                    ? SkeletonLanguage::cpp
                                    : SkeletonLanguage::javascript);
    static_cast<void>(session->view->on_language_chosen(
        [this](SkeletonLanguage language)
        {
            m_settings.set_value("ui_generate_language", language == SkeletonLanguage::cpp ? "cpp" : "javascript");
            static_cast<void>(m_settings.save());
            show_generate_scripts();
        }));
    // And the script after it for that language, chosen beside it.
    {
        const auto script{generate_script(session->view->language())};
        session->view->set_script(script ? std::optional{oui::utf8_of(script->filename())} : std::nullopt);
    }
    // Not from inside the box's own choice, which would stay open over the dialog.
    static_cast<void>(session->view->on_script_wanted(
        [this, raw]
        { m_application.post([this, language{raw->view->language()}] { choose_generate_script(language); }); }));
    static_cast<void>(session->view->on_script_created(
        [this, raw, weak{std::weak_ptr<InteractiveUiSession>{session}}]
        {
            m_application.post(
                [this, weak, language{raw->view->language()}]
                {
                    if(const auto alive{weak.lock()})
                    {
                        create_generate_script(*alive, language);
                    }
                });
        }));
    static_cast<void>(session->view->on_script_dropped(
        [this, raw]
        {
            m_settings.set_value(generate_script_setting(raw->view->language()), "");
            static_cast<void>(m_settings.save());
            show_generate_scripts();
        }));
    static_cast<void>(session->view->on_generate([this, raw] { generate_ui_code(*raw, raw->view->language()); }));
    auto* const code_editor{&session->view->code()};
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.emplace_back();
    group.documents.back().path = file;
    group.documents.back().chosen_language = std::string{"oui"};
    group.documents.back().interactive_ui = session;
    group.tabs.add_tab(file ? document_tab_title(*file, true) : oui::translate("Commands", "Interactive UI"),
                       std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    m_editors.apply_languages(true);
    m_application.focus(*code_editor);
    raw->view->show(raw->view->code().text());
}

std::optional<std::pair<std::filesystem::path, std::string>>
Workbench::generated_ui_code(const InteractiveUiSession& session, SkeletonLanguage language)
{
    // The description's file as it is now, which saving it as another may have changed: the code loads it.
    const auto file_path{[this, &session]() -> std::optional<std::filesystem::path>
                         {
                             for(auto& each : m_editors.groups())
                             {
                                 for(const auto& document : each.documents)
                                 {
                                     if(document.interactive_ui.get() == &session && document.path)
                                     {
                                         return document.path;
                                     }
                                 }
                             }
                             return std::nullopt;
                         }};
    auto path{file_path()};
    // A description with no file yet is saved first, under the name the code loads it by; not saved, no code.
    if(!path)
    {
        static_cast<void>(m_application.commands().execute("file.save_as"));
        path = file_path();
        if(!path)
        {
            return std::nullopt;
        }
    }
    const auto file{oui::utf8_of(path->filename())};
    auto code{language == SkeletonLanguage::cpp ? cpp_skeleton(session.view->code().text(), file)
                                                : script_skeleton(session.view->code().text(), file)};
    if(code.empty())
    {
        show_error(m_application, oui::translate("InteractiveUi", "Generate"),
                   oui::translate("InteractiveUi", "The description does not read yet: see the line at the bottom."));
        return std::nullopt;
    }
    return std::pair{*path, std::move(code)};
}

void Workbench::generate_ui_code(const InteractiveUiSession& session, SkeletonLanguage language)
{
    const auto generated{generated_ui_code(session, language)};
    if(!generated)
    {
        return;
    }
    const auto& [path, script] = *generated;
    const auto cpp{language == SkeletonLanguage::cpp};
    // A new document, to save where the description is or in the program.
    auto& group{m_editors.active()};
    auto document{make_editor()};
    apply_tab_settings(*document, m_settings);
    document->set_text(script);
    auto& document_reference{*document};
    group.tabs.add_tab(oui::translate("Files", "Untitled"), std::move(document));
    group.documents.emplace_back();
    group.documents.back().chosen_language = std::string{cpp ? "cpp" : "javascript"};
    group.tabs.select(group.tabs.child_count() - 1);
    m_editors.apply_languages(true);
    m_application.focus(document_reference);
    if(const auto after{generate_script(language)})
    {
        run_generate_script(*after, script, language, path);
    }
}

void Workbench::create_generate_script(const InteractiveUiSession& session, SkeletonLanguage language)
{
    const auto generated{generated_ui_code(session, language)};
    // The box shows the script there is, whatever happens here.
    show_generate_scripts();
    if(!generated)
    {
        return;
    }
    InteractiveStart start;
    start.code = generated_code_template;
    start.pasted = generated->second;
    start.generated = GeneratedFrom{language == SkeletonLanguage::cpp ? "cpp" : "javascript", generated->first};
    start_interactive_script(std::move(start));
}

void Workbench::saved_under_new_file(Document& document)
{
    if(!document.interactive || !document.path)
    {
        return;
    }
    document.interactive->module_path = *document.path;
    if(const auto& generated{document.interactive->generated})
    {
        m_settings.set_value(generate_script_setting(generated->language == "cpp" ? SkeletonLanguage::cpp
                                                                                  : SkeletonLanguage::javascript),
                             oui::utf8_of(*document.path));
        static_cast<void>(m_settings.save());
        show_generate_scripts();
    }
}

std::optional<std::filesystem::path> Workbench::generate_script(SkeletonLanguage language) const
{
    const auto kept{m_settings.value(generate_script_setting(language))};
    if(kept.empty())
    {
        return std::nullopt;
    }
    return oui::path_from_utf8(kept);
}

void Workbench::show_generate_scripts()
{
    for(auto& group : m_editors.groups())
    {
        for(const auto& document : group.documents)
        {
            if(document.interactive_ui)
            {
                auto& view{*document.interactive_ui->view};
                const auto script{generate_script(view.language())};
                view.set_script(script ? std::optional{oui::utf8_of(script->filename())} : std::nullopt);
            }
        }
    }
}

void Workbench::choose_generate_script(SkeletonLanguage language)
{
    const auto title{oui::translate("InteractiveUi", "Script After Generate")};
    const auto current{generate_script(language)};
    auto options{
        file_dialog_options(oui::FileDialogMode::open_file, current ? current->parent_path() : m_current_folder)};
    options.title = title;
    options.placement_key = "ui_generate_script";
    options.filters = {{oui::translate("ScriptRun", "JavaScript (*.js)"), "*.js"},
                       {oui::translate("Files", "All files (*)"), "*"}};
    const auto script{oui::file_dialog(m_application, options)};
    if(script)
    {
        // Loaded to see that it is one: a script of another kind is not kept.
        ScriptRun checker{m_application, {[](std::string_view) {}, [](std::optional<std::string>) {}}};
        if(const auto problem{checker.load(*script)})
        {
            show_error(m_application, title, *problem);
        }
        else if(checker.kind() != ScriptRun::Kind::on_generated_code)
        {
            show_error(m_application, title,
                       oui::format_text(oui::translate("InteractiveUi",
                                                       "{0} does not export processGeneratedCode(code, {language, "
                                                       "file}), which is what runs after Generate."),
                                        {oui::utf8_of(script->filename())}));
        }
        else
        {
            m_settings.set_value(generate_script_setting(language), oui::utf8_of(*script));
            static_cast<void>(m_settings.save());
        }
    }
    // What there is now, chosen or as it was.
    show_generate_scripts();
}

void Workbench::open_terminal()
{
    oui::PtyOptions options;
    if(m_project_folder)
    {
        options.working_directory = *m_project_folder;
    }
    std::unique_ptr<oui::TerminalView> terminal;
    try
    {
        terminal = std::make_unique<oui::TerminalView>(m_application, std::move(options));
    }
    catch(const std::exception& error)
    {
        show_error(m_application, oui::translate("Commands", "Terminal"),
                   oui::format_text(oui::translate("Files", "Could not start a terminal: {0}"), {error.what()}));
        return;
    }
    auto* const view{terminal.get()};
    // Where the terminal is now: the user may have moved it to another group.
    const auto find_tab{[this, view]() -> std::pair<EditorGroup*, std::size_t>
                        {
                            for(auto& group : m_editors.groups())
                            {
                                for(std::size_t index{}; index < group.documents.size(); ++ index)
                                {
                                    if(group.tabs.content_at<oui::TerminalView>(index) == view)
                                    {
                                        return {&group, index};
                                    }
                                }
                            }
                            return {nullptr, 0};
                        }};
    view->on_title_changed(
        [find_tab](const std::string& title)
        {
            if(const auto [group, index]{find_tab()}; group != nullptr && !title.empty())
            {
                group->tabs.set_title(index, title);
            }
        });
    view->on_finished(
        [this, find_tab](int)
        {
            // Not from inside the terminal's own call, which closing the tab destroys.
            m_application.post(
                [this, find_tab]
                {
                    const auto [group, index]{find_tab()};
                    if(group == nullptr)
                    {
                        return;
                    }
                    group->documents.erase(group->documents.begin() + static_cast<std::ptrdiff_t>(index));
                    group->tabs.close_tab(index);
                    settle_group(m_editors, *group, m_settings);
                    focus_current_tab(m_application, m_editors.active().tabs);
                });
        });
    auto& group{m_editors.active()};
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.emplace_back();
    group.tabs.add_tab(oui::translate("Commands", "Terminal"), std::move(terminal));
    group.tabs.select(group.tabs.child_count() - 1);
    m_application.focus(*view);
}

void Workbench::write_template(std::string_view text)
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    auto* const editor{selected_document_editor(group.tabs)};
    if(editor == nullptr || index >= group.documents.size() || group.documents[index].preview_of ||
       editor->is_read_only())
    {
        show_error(m_application, oui::translate("ScriptRun", "Script Templates"),
                   oui::translate("ScriptRun", "A template goes in a document: "
                                               "open one first."));
        return;
    }
    editor->paste(text);
    // A new document is coloured as what it now is.
    auto& document{group.documents[index]};
    if(!document.path && !document.chosen_language)
    {
        document.chosen_language = "javascript";
        m_editors.apply_languages(true);
    }
}
