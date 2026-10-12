#include "documents.h"

#include "dialogs.h"

#include <oui/oui.h>

using namespace olagarro;

oui::TextEdit* document_editor(oui::TabView& tabs, std::size_t index)
{
    if(auto* const editor{tabs.content_at<oui::TextEdit>(index)})
    {
        return editor;
    }
    if(auto* const interactive{tabs.content_at<InteractiveView>(index)})
    {
        return &interactive->code();
    }
    if(auto* const preview{tabs.content_at<UiPreviewView>(index)})
    {
        return &preview->code();
    }
    return nullptr;
}

oui::TextEdit* selected_document_editor(oui::TabView& tabs)
{
    return document_editor(tabs, tabs.selected_index());
}

std::string title_for(const std::filesystem::path& path)
{
    const auto title{oui::utf8_of(path.filename())};
    return title.empty() ? oui::translate("Files", "Untitled") : title;
}

std::string document_tab_title(const std::filesystem::path& path, bool interactive)
{
    return interactive ? oui::format_text(oui::translate("ScriptRun", "{0} (interactive)"), {title_for(path)})
                       : title_for(path);
}

std::string shown_path(const std::filesystem::path& path)
{
    auto text{oui::utf8_of(path)};
#ifndef _WIN32
    const auto home{oui::utf8_of(oui::known_folder("home"))};
    if(!home.empty() && text.size() > home.size() && text.compare(0, home.size(), home) == 0 &&
       text[home.size()] == '/')
    {
        text = "~" + text.substr(home.size());
    }
#endif
    return text;
}

std::string fit_from_start(std::string text, int width)
{
    if(oui::text_width(text) <= width)
    {
        return text;
    }
    while(!text.empty() && oui::text_width(text) + 1 > width)
    {
        std::size_t next{};
        static_cast<void>(oui::decode_utf8(text, next));
        text.erase(0, next);
    }
    return "…" + text;
}

std::unique_ptr<oui::TextEdit> make_editor()
{
    auto editor{oui::text_edit()};
    editor->set_line_numbers_visible(true);
    editor->set_auto_indent(true);
    editor->set_occurrences_shown(true);
    return editor;
}

void apply_tab_settings(oui::TextEdit& editor, oui::Settings& settings)
{
    editor.set_use_spaces(settings.bool_value("tab_use_spaces", false));
    editor.set_tab_spaces(settings.int_value("tab_space_count", 4));
    editor.set_word_wrap(settings.bool_value("word_wrap", false));
}

bool save_document(oui::Application& application, Document& document, oui::TextEdit& editor,
                   const std::filesystem::path& target)
{
    if(document.path && *document.path == target && document.stamp)
    {
        const auto current{oui::file_stamp(target)};
        if(current && *current != *document.stamp &&
           oui::message_box(
               application, oui::translate("Files", "File Changed on Disk"),
               oui::format_text(oui::translate("Files",
                                               "{0} was changed by another program since it was opened.\nSaving "
                                               "replaces those changes with yours."),
                                {title_for(target)}),
               {oui::translate("Files", "Save Anyway"), oui::translate("Files", "Cancel")}) !=
               std::optional<std::size_t>{0})
        {
            return false;
        }
    }
    auto format{document.format};
    if(!oui::encode_text(editor.text(), format))
    {
        if(oui::message_box(
               application, oui::translate("Files", "Encoding"),
               oui::format_text(
                   oui::translate("Files", "The text has characters that {0} cannot store.\nSave it as UTF-8 instead?"),
                   {oui::encoding_name(format.encoding)}),
               {oui::translate("Files", "Save as UTF-8"), oui::translate("Files", "Cancel")}) !=
           std::optional<std::size_t>{0})
        {
            return false;
        }
        format.encoding = oui::TextEncoding::utf8;
    }
    oui::FileError error;
    const auto stamp{oui::save_text_file(target, editor.text(), format, error)};
    if(!stamp)
    {
        show_error(application, oui::translate("Files", "Save"),
                   oui::format_text(oui::translate("Files", "Could not save {0}:\n{1}"),
                                    {oui::utf8_of(target), oui::describe(error)}));
        return false;
    }
    document.path = target;
    document.format = format;
    document.stamp = stamp;
    editor.mark_saved();
    return true;
}

void check_external_change(oui::Application& application, Document& document, oui::TextEdit& editor,
                           bool reload_automatically)
{
    if(!document.path || !document.stamp)
    {
        return;
    }
    const auto current{oui::file_stamp(*document.path)};
    if(!current || *current == *document.stamp)
    {
        return;
    }
    const auto reload{[&application, &document, &editor]
                      {
                          oui::FileError error;
                          const auto loaded{oui::load_text_file(*document.path, error)};
                          if(!loaded)
                          {
                              show_error(application, oui::translate("Files", "Reload"),
                                         oui::format_text(oui::translate("Files", "Could not reload {0}:\n{1}"),
                                                          {oui::utf8_of(*document.path), oui::describe(error)}));
                              return;
                          }
                          const auto line{editor.line_column().first};
                          editor.set_text(loaded->content.text);
                          static_cast<void>(editor.go_to_line(line));
                          document.format = loaded->content.format;
                          document.stamp = loaded->stamp;
                      }};
    if(!editor.is_modified() && reload_automatically)
    {
        reload();
        return;
    }
    const auto answer{oui::message_box(
        application, oui::translate("Files", "File Changed on Disk"),
        oui::format_text(
            editor.is_modified()
                ? oui::translate("Files",
                                 "{0} was changed by another program.\nReloading it discards your unsaved changes.")
                : oui::translate("Files", "{0} was changed by another program."),
            {title_for(*document.path)}),
        {oui::translate("Files", "Reload"), oui::translate("Files", "Keep Mine")})};
    if(answer == std::optional<std::size_t>{0})
    {
        reload();
    }
    else
    {
        // Keeping yours: do not ask again about this same change, and let a later save overwrite it.
        document.stamp = current;
    }
}
