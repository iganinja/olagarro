#include "workbench.h"

#include "dialogs.h"
#include "help.h"
#include "task_list.h"

#include <oui/oui.h>
#include <oui/terminal_themes.h>
#include <oui/terminal_view.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace olagarro;

void Workbench::open_markdown_preview(oui::TextEdit& editor, const std::string& source_title)
{
    auto& group{m_editors.active()};
    auto view{oui::markdown_view(std::string{editor.text()})};
    // Black, as the help, to tell it from the documents.
    view->set_background(&oui::Theme::reading_background);
    view->set_foreground(&oui::Theme::reading_foreground);
    auto& view_reference{*view};
    Document preview;
    preview.preview_of = &editor;
    // It follows the document as it is edited.
    preview.preview_updates =
        editor.on_changed([&view_reference, &editor] { view_reference.set_markdown(std::string{editor.text()}); });
    // And the links followed in it take it where they lead.
    preview.reading = std::make_shared<PreviewReading>();
    preview.reading->view = &view_reference;
    preview.reading->source = &editor;
    preview.reading->source_title = source_title;
    static_cast<void>(view_reference.on_link(
        [this, weak{std::weak_ptr<PreviewReading>{preview.reading}}](const std::string& target)
        {
            if(const auto reading{weak.lock()})
            {
                follow_preview_link(*reading, target);
            }
        }));
    group.documents.push_back(std::move(preview));
    group.tabs.add_tab(oui::format_text(oui::translate("Files", "Preview: {0}"), {source_title}), std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    m_application.focus(view_reference);
}

std::optional<std::pair<EditorGroup*, std::size_t>> Workbench::preview_tab(const PreviewReading& reading)
{
    for(auto& group : m_editors.groups())
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            if(group.documents[index].reading.get() == &reading)
            {
                return std::pair{&group, index};
            }
        }
    }
    return std::nullopt;
}

void Workbench::show_preview_place(PreviewReading& reading, const PreviewReading::Place& place)
{
    const auto tab{preview_tab(reading)};
    if(!tab)
    {
        return;
    }
    auto& [group, index] = *tab;
    auto& document{group->documents[index]};
    if(place.file)
    {
        // Read first: one that cannot be read leaves the preview as it was.
        const auto text{oui::read_file(*place.file)};
        if(!text)
        {
            show_error(m_application, oui::translate("Files", "Open File"),
                       oui::format_text(oui::translate("Files", "Could not open {0}:\n{1}"),
                                        {oui::utf8_of(*place.file), oui::translate("Files", "It cannot be read.")}));
            return;
        }
        document.preview_updates.disconnect();
        reading.view->set_markdown(*text);
        reading.file = place.file;
        group->tabs.set_title(index,
                              oui::format_text(oui::translate("Files", "Preview: {0}"), {title_for(*place.file)}));
    }
    else if(auto* const source{reading.source.get()})
    {
        // The document again, followed as it is edited.
        auto* const view{reading.view};
        reading.view->set_markdown(std::string{source->text()});
        document.preview_updates.disconnect();
        document.preview_updates =
            source->on_changed([view, source] { view->set_markdown(std::string{source->text()}); });
        reading.file.reset();
        group->tabs.set_title(index, oui::format_text(oui::translate("Files", "Preview: {0}"), {reading.source_title}));
    }
    reading.view->scroll_to(place.scroll);
}

void Workbench::follow_preview_link(PreviewReading& reading, const std::string& target)
{
    // A web address, or mail: the system's own program.
    if(target.find("://") != std::string::npos || target.rfind("mailto:", 0) == 0)
    {
        if(!oui::open_externally(target))
        {
            show_error(m_application, oui::translate("Files", "Open Link"),
                       oui::format_text(oui::translate("Files", "Nothing could open {0}."), {target}));
        }
        return;
    }
    // A file, from the folder of the one shown, and maybe a section of it.
    const auto hash{target.find('#')};
    const auto file_part{target.substr(0, hash)};
    const auto anchor{hash == std::string::npos ? std::string{} : target.substr(hash + 1)};
    std::filesystem::path folder{m_current_folder};
    if(reading.file)
    {
        folder = reading.file->parent_path();
    }
    else if(const auto* const source{reading.source.get()})
    {
        for(auto& group : m_editors.groups())
        {
            for(std::size_t index{}; index < group.documents.size(); ++ index)
            {
                if(document_editor(group.tabs, index) == source && group.documents[index].path)
                {
                    folder = group.documents[index].path->parent_path();
                }
            }
        }
    }
    const auto file{oui::normal_path(folder / oui::path_from_utf8(file_part))};
    auto extension{oui::utf8_of(file.extension())};
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    if(extension != ".md" && extension != ".markdown")
    {
        static_cast<void>(
            open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder, file, m_settings));
        return;
    }
    const PreviewReading::Place was{reading.file, reading.view->scroll_offset()};
    show_preview_place(reading, {file, 0});
    // Moved only when it was read.
    if(reading.file == file)
    {
        reading.back.push_back(was);
        reading.forward.clear();
        if(!anchor.empty())
        {
            static_cast<void>(reading.view->scroll_to_anchor(anchor));
        }
    }
}

bool Workbench::step_preview(bool back)
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    if(index >= group.documents.size() || !group.documents[index].reading)
    {
        return false;
    }
    auto& reading{*group.documents[index].reading};
    auto& from{back ? reading.back : reading.forward};
    auto& to{back ? reading.forward : reading.back};
    if(from.empty())
    {
        return false;
    }
    const auto place{from.back()};
    from.pop_back();
    to.push_back({reading.file, reading.view->scroll_offset()});
    show_preview_place(reading, place);
    return true;
}

HelpTopic Workbench::help_topic()
{
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    if(index >= group.documents.size())
    {
        return HelpTopic::general;
    }
    const auto& document{group.documents[index]};
    if(document.holds_calculator)
    {
        return HelpTopic::calculator;
    }
    if(document.interactive)
    {
        return HelpTopic::interactive_scripts;
    }
    if(document.interactive_ui)
    {
        return HelpTopic::interactive_ui;
    }
    if(document.comparison)
    {
        return HelpTopic::comparing;
    }
    if(document.holds_find_results)
    {
        return HelpTopic::searching;
    }
    if(group.tabs.content_at<TasksView>(index) != nullptr)
    {
        return HelpTopic::tasks;
    }
    if(group.tabs.content_at<oui::TerminalView>(index) != nullptr)
    {
        return HelpTopic::terminal;
    }
    return HelpTopic::general;
}

void Workbench::register_view_commands()
{
    m_application.commands().register_command("view.open_menu", oui::translate("Commands", "Open Menu"),
                                              [this] { m_menu_bar->open_menu(0); });
    m_application.commands().register_command("view.toggle_menu", oui::translate("Commands", "Toggle Menu Bar"),
                                              [this] { m_menu_bar->set_visible(!m_menu_bar->is_visible()); });
    m_application.commands().register_command("view.toggle_project", oui::translate("Commands", "Toggle Project Tree"),
                                              [this] { m_project_tree->set_visible(!m_project_tree->is_visible()); });
    m_application.commands().register_command("view.toggle_explorer", oui::translate("Commands", "Toggle Explorer"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};
                                                  auto& documents{group.documents};

                                                  if(m_file_explorer->is_visible())
                                                  {
                                                      m_file_explorer->set_visible(false);
                                                      focus_current_tab(m_application, tabs_reference);
                                                      return;
                                                  }
                                                  // The first time it needs a folder: the current file's, or the one
                                                  // OUI Code started in.
                                                  if(m_file_explorer->current_path().empty())
                                                  {
                                                      const auto index{tabs_reference.selected_index()};
                                                      std::error_code error;
                                                      m_file_explorer->show_folder(
                                                          index < documents.size() && documents[index].path
                                                              ? documents[index].path->parent_path()
                                                              : std::filesystem::current_path(error));
                                                  }
                                                  m_file_explorer->set_visible(true);
                                                  m_application.focus(*m_file_explorer);
                                              });
    m_application.commands().register_command(
        "view.focus_explorer", oui::translate("Commands", "Focus Next Panel"),
        [this]
        {
            // Round the visible ones: project tree, editor groups, explorer.
            std::vector<oui::Widget*> stops;
            if(m_project_tree->is_visible())
            {
                stops.push_back(m_project_tree);
            }
            // The groups as the screen shows them, top to bottom and left to right.
            std::vector<EditorGroup*> in_order;
            for(auto& group : m_editors.groups())
            {
                in_order.push_back(&group);
            }
            std::sort(in_order.begin(), in_order.end(),
                      [](const EditorGroup* left, const EditorGroup* right)
                      {
                          const auto a{left->tabs.bounds()};
                          const auto b{right->tabs.bounds()};
                          return std::pair{a.y, a.x} < std::pair{b.y, b.x};
                      });
            for(auto* const group : in_order)
            {
                if(auto* const content{group->tabs.selected_content<oui::Widget>()})
                {
                    stops.push_back(content);
                }
            }
            if(m_file_explorer->is_visible())
            {
                stops.push_back(m_file_explorer);
            }
            if(stops.empty())
            {
                return;
            }
            // A stop holds the focus when anything inside it does, as a task list's search field does.
            const auto current{std::find_if(stops.begin(), stops.end(), [this](const oui::Widget* stop)
                                            { return stop->contains(m_application.focused()); })};
            auto* const next{*(current == stops.end() || current + 1 == stops.end() ? stops.begin() : current + 1)};
            if(auto* const tasks{dynamic_cast<TasksView*>(next)})
            {
                tasks->focus_list();
                return;
            }
            // A group's tab is entered as when it is chosen: its editor, its code or its prompt takes the focus, not
            // the tab's content as a whole, which takes no key.
            for(auto* const group : in_order)
            {
                if(group->tabs.selected_content<oui::Widget>() == next)
                {
                    focus_current_tab(m_application, group->tabs);
                    return;
                }
            }
            m_application.focus(*next);
        });
    // Ctrl+K asks with an arrow where to split: the new group goes on that side of the active one, taking the
    // current tab when the group has others and starting blank when not. Shift and an arrow move the tab to
    // the group on that side, Alt and the arrows resize, and Backspace merges the group into the one beside.
    m_application.commands().register_command(
        "view.split_editor", oui::translate("Commands", "Split Editor..."),
        [this]
        {
            const auto several{m_editors.count() > 1};
            // One key and done: an arrow splits there, Shift and an arrow move the tab there, Backspace merges the
            // group into the one beside. Alt and an arrow move the line beside the group, as often as wanted, after
            // which only Alt and arrows do anything. Enter and Escape end it, and any other key is left to do what it
            // does.
            auto resizing{std::make_shared<bool>(false)};
            m_application.start_key_mode(
                several ? oui::translate("SplitEditor", "Split: ←↑→↓ · Move this tab to another group: Shift+←↑→↓ · "
                                                        "Size: Alt+←↑→↓ · Merge: Backspace · Done: Enter")
                        : oui::translate("SplitEditor", "Split: ←↑→↓ · Esc"),
                [this, several, resizing](const oui::Event& event)
                {
                    const auto* const key{std::get_if<oui::KeyEvent>(&event)};
                    // Enter and Escape only end it, so finishing a resize types nothing.
                    if(key != nullptr && (key->key == oui::Key::escape || key->key == oui::Key::enter))
                    {
                        return oui::KeyModeAnswer::end;
                    }
                    const auto choice{key == nullptr || key->control ? std::nullopt : split_choice_of(*key, several)};
                    if(several && key != nullptr && key->alt && !key->shift && choice &&
                       choice->kind == SplitChoice::Kind::split)
                    {
                        m_editors.move_line(m_editors.active(), choice->side);
                        *resizing = true;
                        return oui::KeyModeAnswer::keep;
                    }
                    if(!choice || key->alt || *resizing)
                    {
                        return oui::KeyModeAnswer::end_and_pass;
                    }
                    apply_split_choice(m_application, m_editors, m_settings, *choice);
                    return oui::KeyModeAnswer::end;
                });
        });
    m_application.commands().bind("view.split_editor", {U'k', true});
    // Alt+K moves round the window: each arrow goes to what is nearest on that side, another group, a part of the
    // tab, as the code and the result of an interactive one, or a side panel; Page Up and Page Down go to the tab
    // before or after in the group reached. As often as wanted: Escape and Enter only end it, and any other key ends
    // it and does what it does, so typing goes on where it got to.
    m_application.commands().register_command(
        "view.go_to_group", oui::translate("Commands", "Move Around the Window..."),
        [this]
        {
            m_application.start_key_mode(
                oui::translate("SplitEditor", "Move: ←↑→↓ · Tabs: PgUp/PgDn · Done: any other key"),
                [this](const oui::Event& event)
                {
                    const auto* const key{std::get_if<oui::KeyEvent>(&event)};
                    if(key != nullptr && (key->key == oui::Key::escape || key->key == oui::Key::enter))
                    {
                        return oui::KeyModeAnswer::end;
                    }
                    if(key == nullptr || key->control || key->shift || key->alt)
                    {
                        return oui::KeyModeAnswer::end_and_pass;
                    }
                    switch(key->key)
                    {
                    case oui::Key::left:
                        static_cast<void>(m_application.focus_toward(-1, 0));
                        break;
                    case oui::Key::right:
                        static_cast<void>(m_application.focus_toward(1, 0));
                        break;
                    case oui::Key::up:
                        static_cast<void>(m_application.focus_toward(0, -1));
                        break;
                    case oui::Key::down:
                        static_cast<void>(m_application.focus_toward(0, 1));
                        break;
                    case oui::Key::page_up:
                        step_tab(-1);
                        break;
                    case oui::Key::page_down:
                        step_tab(1);
                        break;
                    default:
                        return oui::KeyModeAnswer::end_and_pass;
                    }
                    return oui::KeyModeAnswer::keep;
                });
        });
    m_application.commands().bind("view.go_to_group", {U'k', false, true});
    // The way out of a terminal tab, which keeps every other key for its programs.
    m_application.commands().reserve("view.go_to_group");
    m_application.commands().register_command("view.word_wrap", oui::translate("Commands", "Toggle Word Wrap"),
                                              [this]
                                              {
                                                  auto& group{m_editors.active()};
                                                  auto& tabs_reference{group.tabs};

                                                  if(auto* const current{selected_document_editor(tabs_reference)})
                                                  {
                                                      const auto enabled{!current->word_wrap()};
                                                      m_settings.set_bool("word_wrap", enabled);
                                                      m_application.commands().set_checked("view.word_wrap", enabled);
                                                      static_cast<void>(m_settings.save());
                                                      for(auto& each : m_editors.groups())
                                                      {
                                                          for(std::size_t index{}; index < each.tabs.child_count();
                                                              ++ index)
                                                          {
                                                              if(auto* const editor{document_editor(each.tabs, index)})
                                                              {
                                                                  editor->set_word_wrap(enabled);
                                                              }
                                                          }
                                                      }
                                                  }
                                              });
    m_application.commands().set_checked("view.word_wrap", m_settings.get_bool("word_wrap", false));
    m_application.commands().register_command("help.shortcuts", oui::translate("Commands", "Keyboard Shortcuts"),
                                              [this] { show_palette(m_application); });
    m_application.commands().register_command(
        "help.contents", oui::translate("Commands", "Help Contents"),
        [this]
        {
            show_help(m_application, help_markdown(m_options.interface_language, m_application.commands()),
                      help_section(help_topic()));
        });
    m_application.commands().bind("help.contents", {oui::Key::f1});
    m_application.commands().register_command(
        "view.markdown_preview", oui::translate("Commands", "Markdown Preview"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            auto& documents{group.documents};

            const auto index{tabs_reference.selected_index()};
            if(index >= documents.size())
            {
                return;
            }
            // On a preview, back to the document it shows.
            if(const auto* const source{documents[index].preview_of.get()})
            {
                for(std::size_t tab{}; tab < tabs_reference.child_count(); ++ tab)
                {
                    if(auto* const editor{document_editor(tabs_reference, tab)}; editor == source)
                    {
                        tabs_reference.select(tab);
                        m_application.focus(*editor);
                    }
                }
                return;
            }
            auto* const editor{selected_document_editor(tabs_reference)};
            if(editor == nullptr)
            {
                return;
            }
            // A document's preview is shown again rather than opened twice.
            for(std::size_t tab{}; tab < documents.size(); ++ tab)
            {
                if(documents[tab].preview_of.refers_to(editor))
                {
                    tabs_reference.select(tab);
                    return;
                }
            }
            open_markdown_preview(*editor, std::string{tabs_reference.title(index)});
        });
    m_application.commands().bind("view.markdown_preview", {oui::Key::f9});
    // From a preview, the file it shows open to edit: the one a link took it to, or the document it previews.
    m_application.commands().register_command(
        "view.edit_previewed", oui::translate("Commands", "Edit the Previewed File"),
        [this]
        {
            auto& group{m_editors.active()};
            const auto index{group.tabs.selected_index()};
            if(index >= group.documents.size() || !group.documents[index].reading)
            {
                return;
            }
            const auto& reading{*group.documents[index].reading};
            if(reading.file)
            {
                static_cast<void>(open_file_at_path(m_application, m_editors, *m_project_tree, m_current_folder,
                                                    *reading.file, m_settings));
                return;
            }
            // The document itself, where its tab is.
            for(auto& each : m_editors.groups())
            {
                for(std::size_t tab{}; tab < each.documents.size(); ++ tab)
                {
                    if(auto* const editor{document_editor(each.tabs, tab)};
                       editor != nullptr && reading.source.refers_to(editor))
                    {
                        m_editors.set_active(each);
                        each.tabs.select(tab);
                        m_application.focus(*editor);
                        return;
                    }
                }
            }
        });
    m_application.commands().bind("view.edit_previewed", {U'e', true, false, true});
    m_application.commands().register_command("help.about", oui::translate("Commands", "About..."),
                                              [this] { show_about(m_application); });
    // Ctrl+M cannot work in a terminal, which sends it as Return; F10 opens the menus instead, as in Turbo C,
    // and reaches the menus where a terminal keeps Alt and a letter for itself.
    m_application.commands().bind("view.open_menu", {oui::Key::f10});
    m_application.commands().bind("view.toggle_project", {U'b', true});
    m_application.commands().bind("view.toggle_explorer", {U'e', true});

    // Shift+F4 lists the tabs, in their order, each with its whole path; Escape goes back to the tab it started on.
    m_application.commands().register_command(
        "view.tab_switcher.open", oui::translate("Commands", "Tab List"),
        [this]
        {
            auto& group{m_editors.active()};
            auto& tabs_reference{group.tabs};
            if(m_tab_switcher.is_open())
            {
                return;
            }
            std::vector<std::string> titles;
            for(std::size_t index{}; index < tabs_reference.child_count(); ++ index)
            {
                std::string label{tabs_reference.title(index)};
                if(index < group.documents.size() && group.documents[index].path)
                {
                    label += "  (" + shown_path(*group.documents[index].path) + ")";
                }
                titles.push_back(std::move(label));
            }
            m_switcher_original_index = tabs_reference.selected_index();
            m_tab_switcher.open(m_application, titles, tabs_reference.selected_index());
        });
    m_tab_switcher.on_confirmed(
        [this](std::size_t row)
        {
            m_editors.active().tabs.select(row);
            m_application.post([this] { focus_current_tab(m_application, m_editors.active().tabs); });
        });
    m_tab_switcher.on_cancelled(
        [this]
        {
            auto& tabs_reference{m_editors.active().tabs};
            if(m_switcher_original_index.has_value())
            {
                tabs_reference.select(*m_switcher_original_index);
            }
            m_switcher_original_index.reset();
            focus_current_tab(m_application, tabs_reference);
        });
    // Ctrl+Page Down and Ctrl+Page Up, and Ctrl+Tab and Ctrl+Shift+Tab where they arrive (in a window: most terminals
    // keep them for their own tabs), go straight to the tab beside, wrapping round at the ends, with nothing to
    // wait for.
    m_application.commands().register_command("view.next_tab", oui::translate("Commands", "Next Tab"),
                                              [this] { step_tab(1); });
    m_application.commands().register_command("view.previous_tab", oui::translate("Commands", "Previous Tab"),
                                              [this] { step_tab(-1); });
    m_application.commands().bind("view.next_tab", {oui::Key::page_down, true});
    m_application.commands().bind("view.previous_tab", {oui::Key::page_up, true});
    m_application.commands().bind("view.next_tab", {oui::Key::tab, true});
    m_application.commands().bind("view.previous_tab", {oui::Key::shift_tab, true});
    m_application.commands().bind("view.tab_switcher.open", {oui::Key::f4, false, false, true});
    m_application.commands().bind("view.focus_explorer", {oui::Key::f4});
#ifdef OLAGARRO_OUI_CODE_WINDOW
    // In a window, Ctrl and plus, minus or the wheel make text and everything with it bigger or smaller, as the
    // application does in every program: the size is kept for the next time.
    if(m_options.window_screen != nullptr)
    {
        m_application.on_zoomed(
            [this]
            {
                m_settings.set("window_zoom", std::to_string(m_options.window_screen->zoom()));
                static_cast<void>(m_settings.save());
            });
    }
#endif
}

std::vector<oui::MenuItem> Workbench::view_menu()
{
    std::vector<oui::MenuItem> view_items{
        "view.command_palette",
        oui::menu_separator(),
        "view.toggle_menu",
        "view.toggle_project",
        "view.toggle_explorer",
        "view.split_editor",
        "view.go_to_group",
        "view.word_wrap",
        "view.markdown_preview",
        "view.code_todos",
        "view.task_list",
        oui::menu_separator(),
        oui::menu_submenu(oui::translate("Menu", "S&yntax"), [this] { return syntax_menu(); })};
#ifdef OLAGARRO_OUI_CODE_WINDOW
    if(m_options.window_screen != nullptr)
    {
        view_items.push_back(oui::menu_separator());
        view_items.push_back("zoom.in");
        view_items.push_back("zoom.out");
        view_items.push_back("zoom.reset");
        // View > Theme, in a window: the graphical looks, kept for the next time.
        view_items.push_back(oui::menu_submenu(oui::translate("Menu", "&Theme"),
                                               [this]
                                               {
                                                   std::vector<oui::MenuItem> items;
                                                   const auto current{m_settings.get("window_theme", "classic")};
                                                   for(const auto& theme : oui::graphical_themes())
                                                   {
                                                       items.push_back(oui::menu_action(
                                                           theme.name,
                                                           [this, id{theme.id}]
                                                           {
                                                               use_window_theme(id);
                                                               m_settings.set("window_theme", id);
                                                               static_cast<void>(m_settings.save());
                                                           },
                                                           true, theme.id == current));
                                                   }
                                                   return items;
                                               }));
    }
#endif
    if(m_options.in_cells)
    {
        // View > Theme, in characters: the terminal looks, kept for the next time.
        view_items.push_back(oui::menu_separator());
        view_items.push_back(oui::menu_submenu(oui::translate("Menu", "&Theme"),
                                               [this]
                                               {
                                                   std::vector<oui::MenuItem> items;
                                                   const auto current{m_settings.get("terminal_theme", "turbo")};
                                                   for(const auto& theme : oui::terminal_themes())
                                                   {
                                                       items.push_back(oui::menu_action(
                                                           theme.name,
                                                           [this, id{theme.id}]
                                                           {
                                                               use_terminal_theme(id);
                                                               m_settings.set("terminal_theme", id);
                                                               static_cast<void>(m_settings.save());
                                                           },
                                                           true, theme.id == current));
                                                   }
                                                   return items;
                                               }));
    }
    return view_items;
}

std::vector<oui::MenuItem> Workbench::syntax_menu()
{
    std::vector<oui::MenuItem> items;
    auto& group{m_editors.active()};
    const auto index{group.tabs.selected_index()};
    if(index >= group.documents.size() || document_editor(group.tabs, index) == nullptr)
    {
        items.push_back(oui::menu_action(oui::translate("Menu", "(none)"), [] {}, false));
        return items;
    }
    auto& document{group.documents[index]};
    const auto choose{[this, &group, index](std::optional<std::string> language)
                      {
                          if(index < group.documents.size())
                          {
                              group.documents[index].chosen_language = std::move(language);
                              m_editors.apply_languages(true);
                          }
                      }};
    const auto own{document.path ? m_scripting->languages().for_path(*document.path) : nullptr};
    items.push_back(oui::menu_action(
        oui::format_text(oui::translate("Menu", "Automatic: {0}"),
                         {own ? own->name : oui::translate("Menu", "Plain Text")}),
        [choose] { choose(std::nullopt); }, true, !document.chosen_language.has_value()));
    items.push_back(oui::menu_action(
        oui::translate("Menu", "Plain Text"), [choose] { choose(std::string{}); }, true,
        document.chosen_language == std::string{}));
    items.push_back(oui::menu_separator());
    auto languages{m_scripting->languages().all()};
    std::sort(languages.begin(), languages.end(),
              [](const auto& left, const auto& right)
              {
                  return std::lexicographical_compare(left->name.begin(), left->name.end(), right->name.begin(),
                                                      right->name.end(), [](unsigned char a, unsigned char b)
                                                      { return std::tolower(a) < std::tolower(b); });
              });
    for(const auto& language : languages)
    {
        items.push_back(oui::menu_action(
            language->name, [choose, id{language->id}] { choose(id); }, true,
            document.chosen_language == language->id));
    }
    return items;
}

void Workbench::step_tab(int step)
{
    auto& tabs_reference{m_editors.active().tabs};
    const auto count{static_cast<int>(tabs_reference.child_count())};
    if(count < 2)
    {
        return;
    }
    const auto current{static_cast<int>(tabs_reference.selected_index())};
    tabs_reference.select(static_cast<std::size_t>((current + step + count) % count));
    focus_current_tab(m_application, tabs_reference);
}
