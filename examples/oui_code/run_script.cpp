#include "workbench.h"

#include "dialogs.h"

#include <oui/oui.h>

#include <memory>
#include <optional>
#include <string>
#include <system_error>

using namespace olagarro;

void Workbench::run_script(bool dry_run, bool choose)
{
    const auto title{oui::translate("ScriptRun", "Run Script")};
    drop_unseen_run();
    if(m_script_run)
    {
        show_error(m_application, title, oui::translate("ScriptRun", "A script is running already; stop it first."));
        return;
    }
    // The document is the current tab's; from the tab that shows a run's output, it is the one that run
    // was on, so F5 and Shift+F5 there go on with it.
    auto* subject_group{&m_editors.active()};
    auto index{subject_group->tabs.selected_index()};
    if(index < subject_group->documents.size() && subject_group->documents[index].holds_script_output)
    {
        for(auto& each : m_editors.groups())
        {
            for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
            {
                if(m_last_script_subject.refers_to(document_editor(each.tabs, tab)))
                {
                    subject_group = &each;
                    index = tab;
                }
            }
        }
    }
    auto& group{*subject_group};
    auto* const editor{document_editor(group.tabs, index)};
    const auto has_document{editor != nullptr && index < group.documents.size() && !group.documents[index].preview_of};
    if(has_document)
    {
        m_last_script_subject = editor;
    }
    const auto document_path{has_document ? group.documents[index].path : std::nullopt};
    std::optional<std::filesystem::path> script;
    if(const auto kept{m_settings.get("run_script")}; !kept.empty() && !choose)
    {
        script = oui::path_from_utf8(kept);
    }
    const auto chosen_now{!script};
    if(chosen_now)
    {
        auto options{file_dialog_options(oui::FileDialogMode::open_file,
                                         document_path ? document_path->parent_path() : m_current_folder)};
        options.title = oui::translate("ScriptRun", "Script to Run");
        options.placement_key = "run_script";
        options.filters = {{oui::translate("ScriptRun", "JavaScript (*.js)"), "*.js"},
                           {oui::translate("Files", "All files (*)"), "*"}};
        script = oui::file_dialog(m_application, options);
        if(!script)
        {
            return;
        }
    }
    auto runner{std::make_unique<ScriptRun>(
        m_application, ScriptRun::Callbacks{[this](std::string_view text) { append_script_output(text); }, {}})};
    // Loaded first, to know which kind it is; one that cannot run is not kept.
    if(const auto problem{runner->load(*script)})
    {
        show_error(m_application, title, *problem);
        return;
    }
    if(chosen_now)
    {
        m_settings.set("run_script", oui::utf8_of(*script));
    }
    const auto script_name{oui::utf8_of(script->filename())};
    if(runner->kind() == ScriptRun::Kind::on_generated_code)
    {
        show_error(m_application, title,
                   oui::format_text(oui::translate("ScriptRun",
                                                   "{0} runs after Generate in an Interactive UI tab, chosen beside "
                                                   "it, not from here."),
                                    {script_name}));
        return;
    }
    if(runner->kind() == ScriptRun::Kind::on_its_own)
    {
        const auto folder{m_project_folder ? *m_project_folder
                          : document_path  ? document_path->parent_path()
                                           : m_current_folder};
        const auto what{dry_run ? oui::translate("ScriptRun", "Preview: {0} in {1}")
                                : oui::translate("ScriptRun", "{0} in {1}")};
        open_script_output(oui::format_text(what, {script_name, title_for(folder)}),
                           oui::format_text(what, {script_name, shown_path(folder)}));
        runner->set_finished(
            [this, dry_run](std::optional<std::string> error)
            {
                m_script_run.reset();
                end_script_output();
                if(error)
                {
                    append_script_output("\n" + *error + "\n");
                    return;
                }
                if(!dry_run)
                {
                    show_script_changes();
                }
                append_script_output("\n" + oui::translate("ScriptRun", "Done.") + "\n");
            });
        m_script_run = std::move(runner);
        m_script_run->run_on_its_own(folder, dry_run);
        return;
    }
    if(!has_document)
    {
        show_error(
            m_application, title,
            oui::format_text(oui::translate("ScriptRun", "{0} runs on a document: open one first."), {script_name}));
        return;
    }
    auto& document{group.documents[index]};
    const std::string original{editor->text()};
    std::filesystem::path target;
    if(dry_run)
    {
        // The copy has the document's name, so a script that looks at it sees what it would on the file.
        std::error_code error;
        const auto folder{std::filesystem::temp_directory_path(error) / oui::path_from_utf8("oui_code_dry_run")};
        std::filesystem::create_directories(folder, error);
        target = folder / (document.path ? document.path->filename() : oui::path_from_utf8("untitled.txt"));
        oui::FileError file_error;
        if(!oui::save_text_file(target, original, document.format, file_error))
        {
            show_error(m_application, title,
                       oui::format_text(oui::translate("Files", "Could not save {0}:\n{1}"),
                                        {oui::utf8_of(target), oui::describe(file_error)}));
            return;
        }
    }
    else
    {
        if(!document.path)
        {
            show_error(m_application, title,
                       oui::translate("ScriptRun", "Save the document first: a script runs on its file."));
            return;
        }
        if(editor->is_modified() && !save_document(m_application, document, *editor, *document.path))
        {
            return;
        }
        target = *document.path;
    }
    const auto name{document.path ? title_for(*document.path) : oui::translate("Files", "Untitled")};
    {
        const auto what{dry_run ? oui::translate("ScriptRun", "Preview: {0} on {1}")
                                : oui::translate("ScriptRun", "{0} on {1}")};
        open_script_output(oui::format_text(what, {script_name, name}),
                           oui::format_text(what, {script_name, document.path ? shown_path(*document.path) : name}));
    }
    const auto language{document.language ? std::optional<std::string>{document.language->id} : std::nullopt};
    runner->set_finished(
        [this, dry_run, target, original, name, language](std::optional<std::string> error)
        {
            m_script_run.reset();
            end_script_output();
            if(error)
            {
                append_script_output("\n" + *error + "\n");
                return;
            }
            if(!dry_run)
            {
                // What the script changed in the file shows in its tab, as another program's change.
                for(auto& each : m_editors.groups())
                {
                    for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
                    {
                        auto* const shown{document_editor(each.tabs, tab)};
                        if(shown != nullptr && each.documents[tab].path == target)
                        {
                            check_external_change(m_application, each.documents[tab], *shown, true);
                        }
                    }
                }
                append_script_output("\n" + oui::translate("ScriptRun", "Done.") + "\n");
                return;
            }
            oui::FileError load_error;
            const auto result{oui::load_text_file(target, load_error)};
            if(!result)
            {
                append_script_output("\n" +
                                     oui::format_text(oui::translate("Files", "Could not open {0}:\n{1}"),
                                                      {oui::utf8_of(target), oui::describe(load_error)}) +
                                     "\n");
                return;
            }
            if(result->content.text == original)
            {
                append_script_output("\n" + oui::translate("ScriptRun", "Done: the script left the text as it was.") +
                                     "\n");
                return;
            }
            auto& into{m_editors.active()};
            auto made{make_editor()};
            apply_tab_settings(*made, m_settings);
            made->set_text(result->content.text);
            made->mark_modified();
            into.documents.emplace_back();
            into.documents.back().format = result->content.format;
            into.documents.back().chosen_language = language;
            // Beside the preview, which stays in front: it says the tab is there.
            const auto result_title{oui::format_text(oui::translate("ScriptRun", "{0} (dry run)"), {name})};
            into.tabs.add_tab(result_title, std::move(made));
            m_editors.apply_languages(true);
            append_script_output(
                "\n" +
                oui::format_text(
                    oui::translate("ScriptRun", "Done: what the script made of the text is in the tab {0}."),
                    {result_title}) +
                "\n");
        });
    m_script_run = std::move(runner);
    m_script_run->run_on_file(target, dry_run);
}

void Workbench::run_generate_script(const std::filesystem::path& script, std::string_view code,
                                    SkeletonLanguage language, const std::filesystem::path& file)
{
    const auto title{oui::translate("InteractiveUi", "Script After Generate")};
    drop_unseen_run();
    if(m_script_run)
    {
        show_error(m_application, title, oui::translate("ScriptRun", "A script is running already; stop it first."));
        return;
    }
    auto runner{std::make_unique<ScriptRun>(
        m_application, ScriptRun::Callbacks{[this](std::string_view text) { append_script_output(text); }, {}})};
    // It may have changed, or gone, since it was chosen: as it is now.
    if(const auto problem{runner->load(script)})
    {
        show_error(m_application, title, *problem);
        return;
    }
    const auto script_name{oui::utf8_of(script.filename())};
    if(runner->kind() != ScriptRun::Kind::on_generated_code)
    {
        show_error(m_application, title,
                   oui::format_text(oui::translate("InteractiveUi",
                                                   "{0} does not export processGeneratedCode(code, {language, "
                                                   "file}), which is what runs after Generate."),
                                    {script_name}));
        return;
    }
    const auto what{oui::translate("ScriptRun", "{0} on {1}")};
    open_script_output(oui::format_text(what, {script_name, title_for(file)}),
                       oui::format_text(what, {script_name, shown_path(file)}));
    runner->set_finished(
        [this](std::optional<std::string> error)
        {
            const auto returned{m_script_run->returned()};
            m_script_run.reset();
            end_script_output();
            if(error)
            {
                append_script_output("\n" + *error + "\n");
                return;
            }
            show_script_changes();
            append_script_output("\n" + (returned ? *returned : oui::translate("ScriptRun", "Done.")) + "\n");
        });
    m_script_run = std::move(runner);
    m_script_run->run_on_generated_code(code, language == SkeletonLanguage::cpp ? "cpp" : "javascript", file);
}

void Workbench::show_script_changes()
{
    // Whatever the script changed shows: open documents, and the folders in the panels.
    for(auto& each : m_editors.groups())
    {
        for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
        {
            if(auto* const shown{document_editor(each.tabs, tab)})
            {
                check_external_change(m_application, each.documents[tab], *shown, true);
            }
        }
    }
    m_project_tree->refresh();
    m_file_explorer->refresh();
}

ScriptOutput* Workbench::script_output_tab()
{
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            if(group.documents[index].holds_script_output)
            {
                return group.tabs.content_at<ScriptOutput>(index);
            }
        }
    }
    return nullptr;
}

void Workbench::drop_unseen_run()
{
    if(m_script_run && script_output_tab() == nullptr)
    {
        m_script_run->stop_programs();
        m_script_run.reset();
    }
}

void Workbench::open_script_output(const std::string& title, const std::string& heading)
{
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            if(auto* const shown{group.documents[index].holds_script_output ? group.tabs.content_at<ScriptOutput>(index)
                                                                            : nullptr})
            {
                group.tabs.set_title(index, title);
                group.tabs.select(index);
                shown->begin(heading);
                m_application.focus(shown->text_view());
                return;
            }
        }
    }
    auto output{std::make_unique<ScriptOutput>()};
    output->on_stop(
        [this]
        {
            if(m_script_run)
            {
                m_script_run->stop_programs();
            }
        });
    output->begin(heading);
    auto& text{output->text_view()};
    auto& group{m_editors.active()};
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.emplace_back();
    group.documents.back().holds_script_output = true;
    group.tabs.add_tab(title, std::move(output));
    group.tabs.select(group.tabs.child_count() - 1);
    m_application.focus(text);
}

void Workbench::append_script_output(std::string_view text)
{
    if(auto* const output{script_output_tab()})
    {
        output->append(text);
    }
    else
    {
        // Not from inside the run's own call.
        m_application.post([this] { drop_unseen_run(); });
    }
}

void Workbench::end_script_output()
{
    if(auto* const output{script_output_tab()})
    {
        output->set_running(false);
    }
}
