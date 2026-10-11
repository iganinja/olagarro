#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What git ignores in a repository, read from its .gitignore files and .git/info/exclude, so that lists of
// the project's files leave out what git leaves out: builds, caches, downloaded packages. It follows the
// patterns as gitignore(5) describes them; the user's global excludes file is not read.
class IgnoreRules
{
  public:
    // The rules that apply inside `folder`: nothing when it is not in a git repository. Otherwise the
    // repository's exclude file and the .gitignore files from its top down to `folder`, both included;
    // those of the folders inside are added with add_folder as a walk reaches them.
    [[nodiscard]] static IgnoreRules for_folder(const std::filesystem::path& folder);
    // Rules read from `text` as the .gitignore of `base`, a folder given relative to the repository's top
    // ("" for the top itself).
    void add(std::string_view text, const std::string& base);
    // Reads the .gitignore of `folder`, when it has one; `inside` is the folder as is_ignored takes paths.
    void add_folder(const std::filesystem::path& folder, const std::string& inside);
    // Whether `folder` lies in a git repository: outside one, nothing is ignored.
    [[nodiscard]] bool in_repository() const;
    // Whether git ignores `path`, given relative to the repository's top with '/' between its parts. A
    // file inside an ignored folder is not looked at: callers stop at the folder.
    [[nodiscard]] bool is_ignored(std::string_view path, bool is_folder) const;
    // `path`, a folder or file inside the repository, as is_ignored wants it.
    [[nodiscard]] std::string relative(const std::filesystem::path& path) const;

  private:
    struct Rule
    {
        std::string pattern;
        // The folder of the .gitignore, relative to the top, with a trailing '/'; empty for the top.
        std::string base;
        bool negated{false};
        bool folders_only{false};
        // A pattern with a '/' before its end matches the path from `base`; one without, the name alone.
        bool anchored{false};
    };

    std::optional<std::filesystem::path> m_root;
    std::vector<Rule> m_rules;
};

// Whether `text` matches a gitignore pattern: '*' and '?' stay within a path part, '[...]' is a set of
// characters, "**" between slashes is any number of folders, and '\' takes the next character as it is.
[[nodiscard]] bool gitignore_match(std::string_view pattern, std::string_view text);
