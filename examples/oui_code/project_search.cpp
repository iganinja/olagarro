#include "project_search.h"

#include "ignore_rules.h"
#include "ui_descriptions.h"

#include <oui/oui.h>

#include <concurrency/concurrency.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <system_error>
#include <utility>

using namespace olagarro;

namespace
{

// The matches of `search` in a text, where `file` says they are.
[[nodiscard]] std::vector<FileMatch> find_in_text(std::string_view text, const std::filesystem::path& file,
                                                  const oui::TextSearch& search)
{
    const auto found{search.find_all(text)};
    std::vector<FileMatch> matches;
    std::size_t line{1};
    std::size_t line_start{};
    std::size_t counted{};
    for(const auto& [start, end] : found)
    {
        for(; counted < start; ++ counted)
        {
            if(text[counted] == '\n')
            {
                ++ line;
                line_start = counted + 1;
            }
        }
        auto line_end{text.find('\n', line_start)};
        if(line_end == std::string_view::npos)
        {
            line_end = text.size();
        }
        matches.push_back({file, line, start - line_start, std::min(end, line_end) - start,
                           std::string{text.substr(line_start, line_end - line_start)}});
    }
    return matches;
}

// The matches of `search` in a file of the project, as it is on disk. Files that cannot be read as text,
// binaries among them, have none; nor do those too large to be source, which would only fill the list.
[[nodiscard]] std::vector<FileMatch> find_in_file(const std::filesystem::path& folder,
                                                  const std::filesystem::path& file, const oui::TextSearch& search)
{
    constexpr std::uintmax_t largest_file{16u << 20u};
    std::error_code size_error;
    if(std::filesystem::file_size(folder / file, size_error) > largest_file || size_error)
    {
        return {};
    }
    oui::FileError error;
    const auto loaded{oui::load_text_file(folder / file, error)};
    if(!loaded || loaded->content.text.find('\0') != std::string::npos)
    {
        return {};
    }
    return find_in_text(loaded->content.text, file, search);
}

// Every file on its own core, as far as there are cores; the matches come out in the order of the files.
[[nodiscard]] std::vector<FileMatch> search_files(const ProjectSearch::Request& request, const oui::TextSearch& search,
                                                  const std::atomic<bool>& stopped)
{
    std::vector<std::vector<FileMatch>> per_file(request.files.size());
    const auto search_file{[&](std::size_t index, const std::filesystem::path& file)
                           {
                               if(stopped.load())
                               {
                                   return;
                               }
                               const auto open{request.unsaved.find(file)};
                               per_file[index] = open != request.unsaved.end()
                                                     ? find_in_text(open->second, file, search)
                                                     : find_in_file(request.folder, file, search);
                           }};
    concurrency::concurrent_for(request.files.begin(), request.files.end(), search_file).get();
    std::vector<FileMatch> matches;
    for(auto& file_matches : per_file)
    {
        std::move(file_matches.begin(), file_matches.end(), std::back_inserter(matches));
    }
    return matches;
}

} // namespace

std::vector<std::filesystem::path> project_files(const std::filesystem::path& folder)
{
    constexpr std::size_t most_files{20000};
    std::vector<std::filesystem::path> files;
    auto ignore{IgnoreRules::for_folder(folder)};
    // Paths as the rules take them: from the top of the repository, which may be above `folder`.
    const auto folder_inside{ignore.relative(folder)};
    const auto inside{[&folder, &folder_inside](const std::filesystem::path& path)
                      {
                          const auto below{oui::utf8_of(oui::relative_to(path, folder))};
                          return folder_inside.empty() ? below : folder_inside + "/" + below;
                      }};
    // Each folder's rules join before what is inside it is looked at; a folder that cannot be read, or a link to
    // one, is passed over.
    const std::function<void(const std::filesystem::path&)> walk{
        [&](const std::filesystem::path& here)
        {
            std::error_code error;
            for(const auto& entry : oui::list_directory(here, error))
            {
                if(files.size() >= most_files)
                {
                    return;
                }
                const auto name{oui::utf8_of(entry.path.filename())};
                if(entry.is_directory)
                {
                    if(entry.is_link)
                    {
                        continue;
                    }
                    if(ignore.in_repository())
                    {
                        const auto path{inside(entry.path)};
                        if(name != ".git" && !ignore.is_ignored(path, true))
                        {
                            ignore.add_folder(entry.path, path);
                            walk(entry.path);
                        }
                    }
                    else if(name.front() != '.' && name.rfind("build", 0) != 0 && name != "node_modules")
                    {
                        walk(entry.path);
                    }
                    continue;
                }
                if(ignore.in_repository() ? !ignore.is_ignored(inside(entry.path), false) : name.front() != '.')
                {
                    files.push_back(oui::relative_to(entry.path, folder));
                }
            }
        }};
    walk(folder);
    std::sort(files.begin(), files.end());
    return files;
}

std::optional<FindInFilesQuery> show_find_in_files_dialog(oui::Application& application, FindInFilesQuery last)
{
    auto ui{build_oui_code_ui(OuiCodeUi::find_in_files)};
    auto& query{ui.get<oui::LineEdit>("query")};
    auto& files{ui.get<oui::LineEdit>("files")};
    auto& whole_word{ui.get<oui::CheckButton>("whole_word")};
    auto& regex{ui.get<oui::CheckButton>("regex")};
    auto& match_case{ui.get<oui::CheckButton>("match_case")};
    auto& problem{ui.get<oui::Label>("problem")};
    query.set_text(last.text);
    files.set_text(last.files);
    whole_word.set_checked(last.options.whole_word);
    regex.set_checked(last.options.regex);
    match_case.set_checked(last.options.match_case);
    auto dialog{ui.take_window()};
    auto& dialog_reference{*dialog};
    std::optional<FindInFilesQuery> result;
    const auto accept{
        [&]
        {
            FindInFilesQuery chosen{std::string{query.text()}, std::string{files.text()}, {}};
            chosen.options.whole_word = whole_word.is_checked();
            chosen.options.regex = regex.is_checked();
            chosen.options.match_case = match_case.is_checked();
            if(chosen.text.empty())
            {
                problem.set_text(oui::translate("FindInFiles", "Type what to find."));
                return;
            }
            if(!oui::TextSearch{chosen.text, chosen.options}.is_valid())
            {
                problem.set_text(oui::translate("FindInFiles", "That is not a valid regular expression."));
                return;
            }
            result = std::move(chosen);
            dialog_reference.close(oui::DialogResult::accepted);
        }};
    const auto submitted{query.on_submitted([&accept](std::string_view) { accept(); })};
    const auto files_submitted{files.on_submitted([&accept](std::string_view) { accept(); })};
    static_cast<void>(ui.get<oui::Button>("find").on_clicked(accept));
    static_cast<void>(ui.get<oui::Button>("cancel").on_clicked(
        [&dialog_reference] { dialog_reference.close(oui::DialogResult::cancelled); }));
    static_cast<void>(application.show_modal(std::move(dialog)));
    return result;
}

bool matches_file_patterns(const std::vector<std::string>& patterns, const std::filesystem::path& file)
{
    if(patterns.empty())
    {
        return true;
    }
    const auto path{oui::utf8_of(file)};
    const auto name{oui::utf8_of(file.filename())};
    return std::any_of(patterns.begin(), patterns.end(),
                       [&path, &name](const std::string& pattern) {
                           return oui::wildcard_match(
                               pattern, pattern.find('/') != std::string::npos ? std::string_view{path} : name);
                       });
}

std::vector<std::string> file_patterns(std::string_view text)
{
    std::vector<std::string> patterns;
    std::string pattern;
    for(const auto character : text)
    {
        if(character == ' ' || character == ',' || character == ';')
        {
            if(!pattern.empty())
            {
                patterns.push_back(std::move(pattern));
                pattern.clear();
            }
            continue;
        }
        pattern += character;
    }
    if(!pattern.empty())
    {
        patterns.push_back(std::move(pattern));
    }
    return patterns;
}

ProjectSearch::ProjectSearch(oui::Application& application) : m_application{application}
{
}
ProjectSearch::~ProjectSearch()
{
    stop();
}

void ProjectSearch::start(Request request, oui::TextSearch search, Done done)
{
    stop();
    auto stopped{std::make_shared<std::atomic<bool>>(false)};
    m_stopped = stopped;
    m_work = std::async(std::launch::async,
                        [&application = m_application, stopped, request{std::move(request)}, search{std::move(search)},
                         done{std::move(done)}]() mutable
                        {
                            auto matches{search_files(request, search, *stopped)};
                            application.post(
                                [stopped, done{std::move(done)}, matches{std::move(matches)}]() mutable
                                {
                                    if(!stopped->load())
                                    {
                                        done(std::move(matches));
                                    }
                                });
                        });
}

void ProjectSearch::stop()
{
    if(m_stopped)
    {
        m_stopped->store(true);
    }
    if(m_work.valid())
    {
        m_work.wait();
    }
}
