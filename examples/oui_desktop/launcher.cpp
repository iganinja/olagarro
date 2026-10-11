#include "launcher.h"

#include <oui/desktop.h>

#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

#include <signal.h>
#include <spawn.h>
#include <unistd.h>

using namespace olagarro;

extern char** environ;

void launch(const std::vector<std::string>& command, const std::filesystem::path& socket)
{
    std::vector<std::string> variables;
    const std::string prefix{std::string{oui::desktop_variable} + "="};
    for(auto** entry{environ}; *entry != nullptr; ++ entry)
    {
        if(std::string_view{*entry}.rfind(prefix, 0) != 0)
        {
            variables.emplace_back(*entry);
        }
    }
    variables.push_back(prefix + socket.string());
    std::vector<char*> environment;
    for(auto& variable : variables)
    {
        environment.push_back(variable.data());
    }
    environment.push_back(nullptr);
    auto words{command};
    std::vector<char*> arguments;
    for(auto& word : words)
    {
        arguments.push_back(word.data());
    }
    arguments.push_back(nullptr);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGDEF);
    // The desktop leaves its ended programs to the system; they wait for their own.
    sigset_t defaults;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGCHLD);
    posix_spawnattr_setsigdefault(&attributes, &defaults);
    pid_t pid{};
    if(const auto result{
           posix_spawn(&pid, arguments.front(), nullptr, &attributes, arguments.data(), environment.data())};
       result != 0)
    {
        std::fprintf(stderr, "oui_desktop: cannot start %s: %s\n", arguments.front(), std::strerror(result));
    }
    posix_spawnattr_destroy(&attributes);
}

std::vector<Launcher> launchers_beside(const std::filesystem::path& folder)
{
    std::vector<Launcher> launchers;
    for(const auto& [title, program] :
        {std::pair{"Terminal", OLAGARRO_OUI_TERM_NAME}, std::pair{"OUI Code", OLAGARRO_OUI_CODE_NAME},
         std::pair{"Calculator", OLAGARRO_OUI_CALC_NAME}})
    {
        const auto path{folder / program};
        if(::access(path.c_str(), X_OK) == 0)
        {
            launchers.push_back({title, {path.string(), "--desktop"}});
        }
    }
    return launchers;
}
