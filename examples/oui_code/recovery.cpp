#include "recovery.h"

#include <oui/file_system.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <system_error>

using namespace olagarro;

namespace
{

// Where OUI Code keeps unsaved documents when the terminal closes under it, until the next start.
std::filesystem::path recovery_folder(const oui::Settings& settings)
{
    return settings.file_path().parent_path() / oui::path_from_utf8("oui_code_recovery");
}

// Drops the kept copy of the unsaved documents, from the settings and the disk, without saving the settings.
void forget_unsaved_documents(oui::Settings& settings)
{
    const auto count{settings.int_value("recovery.count", 0)};
    for(int n{}; n < count; ++ n)
    {
        const auto prefix{"recovery." + std::to_string(n) + "."};
        for(const auto* const field : {"file", "path", "title", "encoding", "line_ending", "stamp_time", "stamp_size"})
        {
            settings.remove(prefix + field);
        }
    }
    settings.remove("recovery.count");
    settings.remove("recovery.announce");
    std::error_code ec;
    oui::remove_entry(recovery_folder(settings), ec);
}

// Writes every modified document beside the settings, with what it needs to come back as it was next
// time, in place of the copy written before. It runs every so often while OUI Code runs, on quitting with
// "keep unsaved changes" on, and when the terminal goes away or the system stops OUI Code, when nobody can
// be asked; `announce` says whether the next start tells the user why the documents are back. A write that
// fails has nobody to tell either.
void keep_unsaved_documents(oui::Settings& settings, const Editors& editors, bool announce)
{
    const auto had_copy{settings.int_value("recovery.count", 0) > 0};
    forget_unsaved_documents(settings);
    const auto folder{recovery_folder(settings)};
    int count{};
    for(const auto& [tabs, documents] : editors.groups())
    {
        for(std::size_t i{}; i < tabs.child_count() && i < documents.size(); ++ i)
        {
            const auto* const editor{document_editor(tabs, i)};
            if(editor == nullptr || !editor->is_modified())
            {
                continue;
            }
            const auto name{"document_" + std::to_string(count) + ".txt"};
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            {
                auto* const file{oui::open_file(folder / oui::path_from_utf8(name), "wb")};
                if(file == nullptr)
                {
                    continue;
                }
                const auto text{editor->text()};
                const auto written{std::fwrite(text.data(), 1, text.size(), file) == text.size()};
                if(std::fclose(file) != 0 || !written)
                {
                    continue;
                }
            }
            const auto& document{documents[i]};
            const auto prefix{"recovery." + std::to_string(count) + "."};
            settings.set_value(prefix + "file", name);
            settings.set_value(prefix + "path", document.path ? oui::utf8_of(*document.path) : "");
            settings.set_value(prefix + "title", std::string{tabs.title(i)});
            settings.set_int_value(prefix + "encoding", static_cast<int>(document.format.encoding));
            settings.set_int_value(prefix + "line_ending", static_cast<int>(document.format.line_ending));
            // The file as it was when last loaded or saved, so a change someone made since is still noticed.
            settings.set_value(prefix + "stamp_time", document.stamp
                                                    ? std::to_string(static_cast<long long>(
                                                          document.stamp->modified.time_since_epoch().count()))
                                                    : "");
            settings.set_value(prefix + "stamp_size", document.stamp ? std::to_string(document.stamp->size) : "");
            ++ count;
        }
    }
    if(count > 0)
    {
        settings.set_int_value("recovery.count", count);
        settings.set_bool_value("recovery.announce", announce);
    }
    if(count > 0 || had_copy)
    {
        static_cast<void>(settings.save());
    }
}

} // namespace

Reopened reopen_unsaved_documents(oui::TabView& tabs, std::vector<Document>& documents, oui::Settings& settings)
{
    const auto count{settings.int_value("recovery.count", 0)};
    if(count == 0)
    {
        return Reopened::nothing;
    }
    const auto announce{settings.bool_value("recovery.announce", true)};
    const auto folder{recovery_folder(settings)};
    auto reopened{false};
    for(int n{}; n < count; ++ n)
    {
        const auto prefix{"recovery." + std::to_string(n) + "."};
        const auto text{oui::read_file(folder / oui::path_from_utf8(settings.value(prefix + "file")))};
        if(!text)
        {
            continue;
        }
        Document document;
        if(const auto path{settings.value(prefix + "path")}; !path.empty())
        {
            document.path = oui::path_from_utf8(path);
        }
        document.format.encoding = static_cast<oui::TextEncoding>(settings.int_value(prefix + "encoding"));
        document.format.line_ending = static_cast<oui::LineEnding>(settings.int_value(prefix + "line_ending"));
        if(const auto time{settings.value(prefix + "stamp_time")}, size{settings.value(prefix + "stamp_size")};
           !time.empty() && !size.empty())
        {
            document.stamp = oui::FileStamp{std::filesystem::file_time_type{std::filesystem::file_time_type::duration{
                                                std::stoll(std::string{time})}},
                                            static_cast<std::uintmax_t>(std::stoull(std::string{size}))};
        }
        const auto open{std::find_if(documents.begin(), documents.end(), [&document](const Document& other)
                                     { return document.path && other.path == document.path; })};
        if(open != documents.end())
        {
            const auto index{static_cast<std::size_t>(open - documents.begin())};
            if(auto* const editor{document_editor(tabs, index)})
            {
                editor->set_text(*text);
                editor->mark_modified();
                *open = document;
                reopened = true;
            }
            continue;
        }
        drop_lone_empty_tab(tabs, documents);
        auto editor{make_editor()};
        apply_tab_settings(*editor, settings);
        editor->set_text(*text);
        editor->mark_modified();
        documents.push_back(document);
        tabs.add_tab(std::string{settings.value(prefix + "title")}, std::move(editor));
        reopened = true;
    }
    forget_unsaved_documents(settings);
    static_cast<void>(settings.save());
    if(!reopened)
    {
        return Reopened::nothing;
    }
    return announce ? Reopened::with_notice : Reopened::quietly;
}

UnsavedBackup::UnsavedBackup(oui::Application& application, oui::Settings& settings, const Editors& editors)
    : m_application{application}, m_settings{settings}, m_editors{editors}
{
}

void UnsavedBackup::configure()
{
    m_timer.stop();
    if(m_settings.bool_value("backup_unsaved", true))
    {
        m_timer = m_application.start_timer(std::chrono::seconds{interval_seconds(m_settings)}, [this] { tick(); });
    }
}

void UnsavedBackup::keep(bool announce)
{
    keep_unsaved_documents(m_settings, m_editors, announce);
    m_kept = fingerprint();
}

void UnsavedBackup::forget()
{
    m_timer.stop();
    if(m_settings.int_value("recovery.count", 0) > 0)
    {
        forget_unsaved_documents(m_settings);
        static_cast<void>(m_settings.save());
    }
}

int UnsavedBackup::interval_seconds(const oui::Settings& settings)
{
    return std::clamp(settings.int_value("backup_interval_seconds", default_interval_seconds), 1, 3600);
}

UnsavedBackup::Fingerprint UnsavedBackup::fingerprint() const
{
    Fingerprint result;
    for(const auto& [tabs, documents] : m_editors.groups())
    {
        for(std::size_t i{}; i < tabs.child_count() && i < documents.size(); ++ i)
        {
            if(const auto* const editor{document_editor(tabs, i)}; editor != nullptr && editor->is_modified())
            {
                const auto& path{documents[i].path};
                result.emplace_back(editor, editor->revision(), path ? oui::utf8_of(*path) : std::string{});
            }
        }
    }
    return result;
}

void UnsavedBackup::tick()
{
    if(fingerprint() != m_kept)
    {
        keep(true);
    }
}
