#include "workbench.h"

#include "dialogs.h"
#include "task_list.h"

#include <oui/oui.h>
#include <oui/table.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace olagarro;

void Workbench::register_search_commands()
{
    m_application.commands().register_command(
        "edit.find_in_files", oui::translate("Commands", "Find in Files..."),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};

            if(!m_project_folder)
            {
                show_error(m_application, oui::translate("FindInFiles", "Find in Files"),
                           oui::translate("FindInFiles", "Open a folder first, to look through its files."));
                return;
            }
            // What is selected, when it is on one line, is what the user most likely wants to find.
            auto last{m_last_find_in_files};
            if(const auto* const editor{selected_document_editor(tabs_reference)})
            {
                if(auto selected{editor->selected_text()};
                   !selected.empty() && selected.find('\n') == std::string::npos)
                {
                    last.text = std::move(selected);
                }
            }
            const auto query{show_find_in_files_dialog(m_application, last)};
            if(!query)
            {
                return;
            }
            m_last_find_in_files = *query;
            search_project(*query, oui::format_text(oui::translate("FindInFiles", "Find: {0}"), {query->text}));
        });
    m_application.commands().bind("edit.find_in_files", {U'f', true, false, true});
    // What is still to do in the code: its TODO, FIXME, XXX and HACK comments, listed as Find in Files lists.
    m_application.commands().register_command(
        "view.code_todos", oui::translate("Commands", "Code T&ODOs"),
        [this]
        {
            if(!m_project_folder)
            {
                show_error(m_application, oui::translate("Commands", "Code TODOs"),
                           oui::translate("FindInFiles", "Open a folder first, to look through its files."));
                return;
            }
            FindInFilesQuery query;
            query.text = R"(\b(TODO|FIXME|XXX|HACK)\b)";
            query.options.regex = true;
            query.options.match_case = true;
            search_project(query, oui::translate("Commands", "Code TODOs"));
        });
    // The project's own list of things to do, each with its note, in a tab of its own.
    m_application.commands().register_command(
        "view.task_list", oui::translate("Commands", "Task &List"),
        [this] {
            open_task_list(m_application, m_editors,
                           tasks_file(m_project_folder, m_settings.file_path().parent_path()));
        });
    // Ctrl+T: what a new tab can hold, each kind there is: a document, a shell, the calculator, an interactive script
    // or user interface, a comparison, and the lists of things to do.
    m_application.commands().register_command(
        "view.new_tab", oui::translate("Commands", "New &Tab..."),
        [this]
        {
            const std::vector<std::string> commands{"file.new",
                                                    "tools.terminal",
                                                    "tools.calculator",
                                                    "tools.interactive_script",
                                                    "tools.interactive_ui",
                                                    "tools.compare_files",
                                                    "view.task_list",
                                                    "view.code_todos"};
            std::vector<oui::QuickPickItem> items;
            for(const auto& id : commands)
            {
                const auto shortcut{m_application.commands().shortcut(id)};
                items.push_back({oui::strip_mnemonic(std::string{*m_application.commands().title(id)}),
                                 shortcut ? oui::format_shortcut(*shortcut) : std::string{}});
            }
            oui::QuickPickOptions options;
            options.title = oui::translate("Commands", "New Tab");
            options.fit_to_items = true;
            if(const auto picked{oui::quick_pick(m_application, items, options)})
            {
                static_cast<void>(m_application.commands().execute(commands.at(*picked)));
            }
        });
    m_application.commands().bind("view.new_tab", {U't', true});
}

std::optional<std::pair<EditorGroup*, std::size_t>> Workbench::find_results_tab()
{
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            if(group.documents[index].holds_find_results)
            {
                return std::pair{&group, index};
            }
        }
    }
    return std::nullopt;
}

void Workbench::search_project(const FindInFilesQuery& query, std::string title)
{
    auto& group{m_editors.active()};
    auto& tabs_reference{group.tabs};
    auto& documents{group.documents};
    const auto patterns{file_patterns(query.files)};
    ProjectSearch::Request request{*m_project_folder, {}, {}};
    for(auto& file : project_files(*m_project_folder))
    {
        if(matches_file_patterns(patterns, file))
        {
            request.files.push_back(std::move(file));
        }
    }
    for(const auto& each : m_editors.groups())
    {
        for(std::size_t tab{}; tab < each.tabs.child_count() && tab < each.documents.size(); ++ tab)
        {
            const auto* const editor{document_editor(each.tabs, tab)};
            if(editor != nullptr && editor->is_modified() && each.documents[tab].path)
            {
                request.unsaved.emplace(oui::relative_to(*each.documents[tab].path, *m_project_folder), editor->text());
            }
        }
    }
    oui::Table* table{};
    auto* results_group{&group};
    std::optional<std::size_t> index;
    if(const auto found{find_results_tab()})
    {
        results_group = found->first;
        index = found->second;
        table = results_group->tabs.content_at<oui::Table>(*index);
        table->clear();
    }
    else
    {
        auto results{std::make_unique<oui::Table>()};
        table = results.get();
        table->set_headers({oui::translate("FindInFiles", "File"), oui::translate("FindInFiles", "Text")});
        // The file and line, then the line's text in whatever width is left.
        table->set_column_width(0, 30);
        table->set_column_width(1, 10);
        table->set_stretch_column(1);
        table->set_columns_resizable(true);
        table->on_row_activated(
            [this](std::size_t row)
            {
                if(row >= m_find_results.size() || !m_project_folder)
                {
                    return;
                }
                // With the editor split, the file opens beside the results rather than over them.
                if(auto* const beside{any_neighbour(m_editors, m_editors.active())})
                {
                    focus_current_tab(m_application, beside->tabs);
                }
                const auto match{m_find_results[row]};
                open_from_panel(*m_project_folder / match.file);
                auto* const editor{selected_document_editor(m_editors.active().tabs)};
                if(editor == nullptr || !editor->go_to_line(match.line))
                {
                    return;
                }
                const auto line_start{editor->cursor_offset()};
                editor->select_range(line_start + match.column, line_start + match.column + match.length);
                m_application.focus(*editor);
            });
        Document document;
        document.holds_find_results = true;
        documents.push_back(std::move(document));
        tabs_reference.add_tab("", std::move(results));
        index = tabs_reference.child_count() - 1;
    }
    m_find_results.clear();
    // What the search says goes in the wide column, whatever width the places have.
    table->add_line({"", oui::translate("FindInFiles", "Searching...")});
    results_group->tabs.set_title(*index, title);
    results_group->tabs.select(*index);
    m_application.focus(*table);
    m_project_search.start(
        std::move(request), oui::TextSearch{query.text, query.options},
        [this, title](std::vector<FileMatch> matches)
        {
            const auto found{find_results_tab()};
            if(!found)
            {
                return;
            }
            auto& tabs_reference{found->first->tabs};
            const auto tab{found->second};
            auto* const results{tabs_reference.content_at<oui::Table>(tab)};
            results->clear();
            // Every match is a row of widgets: past a few thousand the list is too long to read
            // anyway, and the search had better be narrowed.
            constexpr std::size_t most_rows{2000};
            const auto shown{std::min(matches.size(), most_rows)};
            // The places' column is as wide as the longest, and never narrower than its header.
            auto width{static_cast<std::size_t>(oui::text_width(oui::translate("FindInFiles", "File")))};
            for(std::size_t row{}; row < shown; ++ row)
            {
                const auto& match{matches[row]};
                const auto place{oui::utf8_of(match.file) + ":" + std::to_string(match.line)};
                width = std::max(width, static_cast<std::size_t>(oui::text_width(place)));
                const auto indent{match.text.find_first_not_of(" \t")};
                results->add_line(
                    {place, indent == std::string::npos ? "" : std::string_view{match.text}.substr(indent)});
            }
            if(matches.empty())
            {
                results->add_line({"", oui::translate("FindInFiles", "No matches.")});
            }
            else if(matches.size() > shown)
            {
                results->add_line(
                    {"", oui::format_text(
                             oui::translate("FindInFiles", "{0} more matches are not shown: narrow the search."),
                             {std::to_string(matches.size() - shown)})});
            }
            // Two cells more, to keep the text apart from the place.
            results->set_column_width(0, static_cast<int>(std::min<std::size_t>(width + 2, 50)));
            tabs_reference.set_title(tab, title + " (" + std::to_string(matches.size()) + ")");
            matches.resize(shown);
            m_find_results = std::move(matches);
        });
}
