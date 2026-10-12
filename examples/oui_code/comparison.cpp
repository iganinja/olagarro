#include "comparison.h"

#include "dialogs.h"
#include "workbench.h"

#include <oui/oui.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

using namespace olagarro;

std::string ComparedSide::current_text() const
{
    if(const auto* const document{editor.get()})
    {
        return std::string{document->text()};
    }
    return text;
}

void Workbench::open_calculator(const std::vector<std::string>& entries)
{
    auto& group{m_editors.active()};
    auto view{std::make_unique<oui::CalculatorView>(m_application)};
    view->replay(entries);
    auto& prompt{view->prompt()};
    // exit written on its line closes its tab, once the key that wrote it is done with the calculator.
    const oui::WidgetRef<oui::CalculatorView> calculator{view.get()};
    static_cast<void>(view->on_exit(
        [this, calculator]
        {
            m_application.post(
                [this, calculator]
                {
                    for(auto& each : m_editors.groups())
                    {
                        for(std::size_t tab{}; tab < each.tabs.child_count(); ++ tab)
                        {
                            if(calculator.refers_to(each.tabs.content_at<oui::CalculatorView>(tab)))
                            {
                                m_editors.set_active(each);
                                each.tabs.select(tab);
                                static_cast<void>(m_application.commands().execute("file.close_tab"));
                                return;
                            }
                        }
                    }
                });
        }));
    Document document;
    document.holds_calculator = true;
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.push_back(std::move(document));
    group.tabs.add_tab(oui::translate("Calculator", "Calculator"), std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    m_application.focus(prompt);
}

std::optional<ComparedSide> Workbench::compared_file(const std::filesystem::path& file)
{
    ComparedSide side;
    side.path = file;
    side.title = shown_path(file);
    side.name = title_for(file);
    // An open document is compared as it is in the editor, and followed as it changes.
    for(auto& group : m_editors.groups())
    {
        for(std::size_t tab{}; tab < group.documents.size(); ++ tab)
        {
            if(group.documents[tab].path == file)
            {
                if(auto* const editor{document_editor(group.tabs, tab)})
                {
                    side.editor = editor;
                    side.text = std::string{editor->text()};
                    return side;
                }
            }
        }
    }
    oui::FileError error;
    const auto loaded{oui::load_text_file(file, error)};
    if(!loaded)
    {
        show_error(m_application, oui::translate("Compare", "Compare Files"),
                   oui::format_text(oui::translate("Files", "Could not open {0}:\n{1}"),
                                    {oui::utf8_of(file), oui::describe(error)}));
        return std::nullopt;
    }
    side.text = loaded->content.text;
    return side;
}

void Workbench::open_comparison(ComparedSide left, ComparedSide right, bool against_saved)
{
    auto& group{m_editors.active()};
    auto view{oui::diff_view()};
    auto& view_reference{*view};
    view->set_tab_width(m_settings.int_value("tab_space_count", 4));
    view->set_titles(left.title, right.title);
    view->set_texts(left.current_text(), right.current_text());

    auto comparison{std::make_shared<Comparison>()};
    comparison->view = &view_reference;
    comparison->left = std::move(left);
    comparison->right = std::move(right);
    comparison->against_saved = against_saved;
    auto* const raw{comparison.get()};
    // Compared again a moment after the last change to either side, not at every key.
    const auto soon{[this, raw]
                    {
                        raw->pending = m_application.start_timer(std::chrono::milliseconds{300},
                                                                 [raw]
                                                                 {
                                                                     raw->pending.stop();
                                                                     raw->view->set_texts(raw->left.current_text(),
                                                                                          raw->right.current_text());
                                                                 });
                    }};
    // A document closed since is kept as it last was.
    for(auto* const side : {&raw->left, &raw->right})
    {
        if(auto* const editor{side->editor.get()})
        {
            auto& connection{side == &raw->left ? raw->left_changed : raw->right_changed};
            connection = signal_slot::ScopedConnection{editor->on_changed(
                [side, soon]
                {
                    side->text = side->current_text();
                    soon();
                })};
        }
    }
    view->on_activated([this, raw](oui::DiffSide chosen, std::optional<std::size_t> line)
                       { go_to_compared_line(*raw, chosen, line); });

    Document document;
    document.comparison = std::move(comparison);
    // In place of the empty document OUI Code starts with, as a file opened is.
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.push_back(std::move(document));
    group.tabs.add_tab(oui::format_text(oui::translate("Compare", "{0} vs {1}"), {raw->left.name, raw->right.name}),
                       std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    m_application.focus(view_reference);
}

void Workbench::go_to_compared_line(Comparison& comparison, oui::DiffSide chosen, std::optional<std::size_t> line)
{
    // The side chosen, or the other one where the chosen one has nothing to go to: no line there, or no document,
    // as the saved side of Compare with Saved.
    const auto& row{comparison.view->rows()[comparison.view->current_row()]};
    const auto navigable{[](const ComparedSide& side) { return side.editor.get() != nullptr || side.path; }};
    auto* side{chosen == oui::DiffSide::left ? &comparison.left : &comparison.right};
    if(!line || !navigable(*side))
    {
        side = chosen == oui::DiffSide::left ? &comparison.right : &comparison.left;
        line = chosen == oui::DiffSide::left ? row.right_line : row.left_line;
    }
    if(!line || !navigable(*side))
    {
        return;
    }
    oui::TextEdit* editor{};
    if(auto* const open{side->editor.get()})
    {
        for(auto& group : m_editors.groups())
        {
            for(std::size_t tab{}; tab < group.tabs.child_count(); ++ tab)
            {
                if(document_editor(group.tabs, tab) == open)
                {
                    group.tabs.select(tab);
                    editor = open;
                }
            }
        }
    }
    else if(side->path &&
            open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder, *side->path, m_settings))
    {
        editor = selected_document_editor(m_editors.active().tabs);
    }
    if(editor != nullptr && editor->go_to_line(*line + 1))
    {
        m_application.focus(*editor);
    }
}

void Workbench::register_compare_commands()
{
    // A calculator in a tab, the one there is shown again rather than another opened.
    m_application.commands().register_command("tools.calculator", oui::translate("Commands", "Ca&lculator"),
                                              [this]
                                              {
                                                  for(auto& group : m_editors.groups())
                                                  {
                                                      for(std::size_t tab{}; tab < group.documents.size(); ++ tab)
                                                      {
                                                          if(group.documents[tab].holds_calculator)
                                                          {
                                                              group.tabs.select(tab);
                                                              focus_current_tab(m_application, group.tabs);
                                                              return;
                                                          }
                                                      }
                                                  }
                                                  open_calculator({});
                                              });

    const auto title{oui::translate("Compare", "Compare Files")};
    // The document being edited against a file chosen for it; with none, two files chosen one after the other.
    m_application.commands().register_command(
        "tools.compare_files", oui::translate("Commands", "&Compare Files..."),
        [this]
        {
            auto& group{m_editors.active()};
            const auto index{group.tabs.selected_index()};
            auto* const editor{selected_document_editor(group.tabs)};
            std::optional<ComparedSide> left;
            if(editor != nullptr && index < group.documents.size())
            {
                ComparedSide side;
                side.path = group.documents[index].path;
                side.editor = editor;
                side.text = std::string{editor->text()};
                side.title = side.path ? shown_path(*side.path) : group.tabs.title(index);
                side.name = side.path ? title_for(*side.path) : group.tabs.title(index);
                left = std::move(side);
            }
            const auto choose{[this](const std::string& dialog_title)
                              {
                                  auto options{file_dialog_options(oui::FileDialogMode::open_file, m_current_folder)};
                                  options.title = dialog_title;
                                  return oui::file_dialog(m_application, options);
                              }};
            if(!left)
            {
                const auto first{choose(oui::translate("Compare", "Compare Which File?"))};
                if(!first)
                {
                    return;
                }
                left = compared_file(*first);
                if(!left)
                {
                    return;
                }
            }
            const auto second{choose(oui::format_text(oui::translate("Compare", "Compare {0} With"), {left->name}))};
            if(!second)
            {
                return;
            }
            auto right{compared_file(*second)};
            if(!right)
            {
                return;
            }
            m_current_folder = second->parent_path();
            open_comparison(std::move(*left), std::move(*right));
        });
    // What the document has that its file does not yet: the file as saved on the left, the document on the right.
    m_application.commands().register_command(
        "tools.compare_saved", oui::translate("Commands", "Compare with &Saved"),
        [this, title]
        {
            auto& group{m_editors.active()};
            const auto index{group.tabs.selected_index()};
            auto* const editor{selected_document_editor(group.tabs)};
            if(editor == nullptr || index >= group.documents.size() || !group.documents[index].path)
            {
                show_error(m_application, title,
                           oui::translate("Compare", "Only a document with a file has a saved version."));
                return;
            }
            compare_with_saved(*editor, *group.documents[index].path);
        });
}

void Workbench::compare_with_saved(oui::TextEdit& editor, const std::filesystem::path& file)
{
    oui::FileError error;
    const auto loaded{oui::load_text_file(file, error)};
    if(!loaded)
    {
        show_error(m_application, oui::translate("Compare", "Compare Files"),
                   oui::format_text(oui::translate("Files", "Could not open {0}:\n{1}"),
                                    {oui::utf8_of(file), oui::describe(error)}));
        return;
    }
    ComparedSide saved;
    saved.text = loaded->content.text;
    const auto saved_pattern{oui::translate("Compare", "{0} (saved)")};
    saved.title = oui::format_text(saved_pattern, {shown_path(file)});
    saved.name = oui::format_text(saved_pattern, {title_for(file)});
    ComparedSide edited;
    edited.path = file;
    edited.editor = &editor;
    edited.text = std::string{editor.text()};
    edited.title = shown_path(file);
    edited.name = title_for(file);
    open_comparison(std::move(saved), std::move(edited), true);
}
