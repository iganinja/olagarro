#pragma once

#include <oui/application.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// A script run live, again each time it changes, as the interactive mode does: it only looks, so it may run half
// typed. What it exports says what it is for:
// - transform(text): gets the input and gives back the text in its place, the result;
// - processFile(file, dryRun): works on a copy of the input, as a dry run, and the result is the copy after;
// - process(dryRun, folder): a dry run in the input's folder, which shows what it would do in its log;
// - processGeneratedCode(code, {language, file, dryRun}): gets the input as code an Interactive UI tab's Generate
//   wrote, as a dry run, and the result is the text it gives back;
// - nothing: what it prints is all there is.
// Writing files, running programs and asking questions throw, and each run has a second to finish.

// What Tools > Interactive Script starts a new script with, for the document it is to work on.
extern const std::string_view interactive_script_template;

// What the input of a processGeneratedCode was generated as: "javascript" or "cpp", from the description at `file`.
struct GeneratedFrom
{
    std::string language;
    std::filesystem::path file;
};

struct LiveResult
{
    // What the script made of the input; none when it gives no text or failed.
    std::optional<std::string> text;
    // What it printed, a line each time.
    std::string log;
    // Why it failed, and on which line of the script when that is known.
    std::optional<std::string> error;
    std::optional<std::size_t> error_line;
};

class LiveScript
{
  public:
    // `done` hears each run's result on the application's thread; only the last run's, a run started since
    // leaves the one before unheard.
    LiveScript(olagarro::oui::Application& application, std::function<void(LiveResult)> done);
    ~LiveScript();
    LiveScript(const LiveScript&) = delete;
    LiveScript& operator=(const LiveScript&) = delete;

    // Runs `code` as the module at `path`, whose folder its imports start from, on `input` when there is one; the
    // input's own file, when it has one, says the folder a process works in; `generated`, what a processGeneratedCode
    // is told the input is, when it is generated code.
    void run(std::string_view code, const std::filesystem::path& path, std::optional<std::string> input,
             std::optional<std::filesystem::path> input_file, const std::optional<GeneratedFrom>& generated);

    struct Run;

  private:
    olagarro::oui::Application& m_application;
    std::function<void(LiveResult)> m_done;
    std::unique_ptr<Run> m_run;
};
