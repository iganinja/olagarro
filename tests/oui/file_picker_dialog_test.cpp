#include <oui/application.h>
#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/file_dialog.h>
#include <oui/file_system.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include <algorithm>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace olagarro;

namespace
{

// An open-file dialog on `folder` with the defaults.
oui::FileDialogOptions open_options(const std::filesystem::path& folder)
{
    oui::FileDialogOptions options;
    options.directory = folder;
    return options;
}

// A terminal-sized backend that closes the dialog with Escape and keeps every frame it was shown.
class RecordingBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {100, 30}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_frames.empty() || m_escaped)
        {
            return std::nullopt;
        }
        m_escaped = true;
        return oui::KeyEvent{oui::Key::escape};
    }

    void present(const oui::CellBuffer& buffer) override { m_frames.push_back(buffer); }

    [[nodiscard]] const std::vector<oui::CellBuffer>& frames() const { return m_frames; }

  private:
    std::vector<oui::CellBuffer> m_frames;
    bool m_escaped{false};
};

// Width of the dialog titled `title`, corners included. It is measured on the bottom border, below the
// title's left corner, because a title wider than the dialog covers the top-right corner.
int dialog_width(const oui::CellBuffer& frame, std::u32string_view title)
{
    const auto row_text{[&frame](int y)
                        {
                            std::u32string row;
                            for(auto x{0}; x < frame.width(); ++ x)
                            {
                                row += frame.cell_at(x, y).glyph;
                            }
                            return row;
                        }};
    for(auto y{0}; y < frame.height(); ++ y)
    {
        const auto top{row_text(y)};
        const auto title_at{top.find(title)};
        if(title_at == std::u32string::npos)
        {
            continue;
        }
        const auto left{top.find_last_of(U"┌╔", title_at)};
        for(auto bottom_y{y + 1}; left != std::u32string::npos && bottom_y < frame.height(); ++ bottom_y)
        {
            const auto bottom{row_text(bottom_y)};
            if(bottom.at(left) == U'└' || bottom.at(left) == U'╚')
            {
                const auto right{bottom.find_first_of(U"┘╝", left)};
                return right == std::u32string::npos ? 0 : static_cast<int>(right - left + 1);
            }
        }
    }
    return 0;
}

TEST(OuiFilePickerDialogTests, OpenDialogUsesItsFullWidthOnAWideTerminal)
{
    auto backend{std::make_unique<RecordingBackend>()};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, open_options(std::filesystem::temp_directory_path())));

    // The dialog is capped at 72 columns; it must not collapse to the places column.
    ASSERT_FALSE(recorder.frames().empty());
    EXPECT_EQ(dialog_width(recorder.frames().front(), U"Open File"), 72);
}

TEST(OuiFilePickerDialogTests, AFolderGivenAsDotDotStillHasAParent)
{
    const auto top{std::filesystem::temp_directory_path() / "oui_dot_dot_test"};
    std::filesystem::create_directories(top / "inner" / "deepest");
    const auto previous{std::filesystem::current_path()};
    std::filesystem::current_path(top / "inner" / "deepest");
    auto backend{std::make_unique<RecordingBackend>()};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    static_cast<void>(oui::file_dialog(application, open_options("..")));
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(top);

    // The location shows the folder whole, and the list starts with the way up from it.
    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    std::string text;
    for(int y{}; y < frame.height(); ++ y)
    {
        for(int x{}; x < frame.width(); ++ x)
        {
            const auto glyph{frame.cell_at(x, y).glyph};
            text += glyph < 128 ? static_cast<char>(glyph) : '?';
        }
        text += '\n';
    }
    EXPECT_NE(text.find(oui::utf8_of(top / "inner")), std::string::npos) << text;
}

#ifndef _WIN32
TEST(OuiFilePickerDialogTests, AFileWithNoSizeShowsNone)
{
    // A named pipe has no size to give: it shows none, not what an error leaves in its place.
    const auto folder{std::filesystem::temp_directory_path() / "oui_no_size_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    std::ofstream{folder / "five.txt"} << "12345";
    ASSERT_EQ(::mkfifo((folder / "pipe").c_str(), 0600), 0);
    auto backend{std::make_unique<RecordingBackend>()};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    static_cast<void>(oui::file_dialog(application, open_options(folder)));
    std::filesystem::remove_all(folder);

    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    const auto row_with{[&frame](std::string_view name)
                        {
                            for(int y{}; y < frame.height(); ++ y)
                            {
                                std::string row;
                                for(int x{}; x < frame.width(); ++ x)
                                {
                                    const auto glyph{frame.cell_at(x, y).glyph};
                                    row += glyph < 128 ? static_cast<char>(glyph) : '?';
                                }
                                if(row.find(name) != std::string::npos)
                                {
                                    return row;
                                }
                            }
                            return std::string{};
                        }};
    EXPECT_NE(row_with("five.txt").find("5 B"), std::string::npos) << row_with("five.txt");
    const auto pipe{row_with("pipe")};
    ASSERT_FALSE(pipe.empty());
    EXPECT_EQ(pipe.find(" B "), std::string::npos) << pipe;
    EXPECT_EQ(pipe.find("GB"), std::string::npos) << pipe;
    EXPECT_EQ(pipe.find('~'), std::string::npos) << pipe;
    // Nor does the way up, a folder.
    const auto up{row_with("[..]")};
    ASSERT_FALSE(up.empty());
    EXPECT_EQ(up.find(" B "), std::string::npos) << up;
}
#endif

TEST(OuiFilePickerDialogTests, ItsLabelsStartInTheSameColumn)
{
    // Location, File name and File type, one under the other, down the dialog's left side.
    auto backend{std::make_unique<RecordingBackend>()};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    static_cast<void>(oui::file_dialog(application, open_options(std::filesystem::temp_directory_path())));

    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    const auto column_of{[&frame](std::string_view text) -> std::optional<std::size_t>
                         {
                             for(int y{}; y < frame.height(); ++ y)
                             {
                                 std::string row;
                                 for(int x{}; x < frame.width(); ++ x)
                                 {
                                     const auto glyph{frame.cell_at(x, y).glyph};
                                     row += glyph < 128 ? static_cast<char>(glyph) : '?';
                                 }
                                 if(const auto at{row.find(text)}; at != std::string::npos)
                                 {
                                     return at;
                                 }
                             }
                             return std::nullopt;
                         }};
    const auto location{column_of("Location:")};
    ASSERT_TRUE(location);
    EXPECT_EQ(location, column_of("File name:"));
    EXPECT_EQ(location, column_of("File type:"));
}

TEST(OuiFilePickerDialogTests, FileTypeArrowSitsAtTheRightEdge)
{
    auto backend{std::make_unique<RecordingBackend>()};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, open_options(std::filesystem::temp_directory_path())));

    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    auto found{false};
    for(auto y{0}; y < frame.height(); ++ y)
    {
        std::u32string row;
        for(auto x{0}; x < frame.width(); ++ x)
        {
            row += frame.cell_at(x, y).glyph;
        }
        if(row.find(U"File type:") == std::u32string::npos)
        {
            continue;
        }
        found = true;
        const auto arrow{row.find(U'▾')};
        ASSERT_NE(arrow, std::u32string::npos);
        const auto border{row.find_first_of(U"│║", arrow)};
        ASSERT_NE(border, std::u32string::npos);
        // Only the dropdown's own padding and the dialog's inner margin separate the arrow from the border.
        EXPECT_LE(border - arrow, 3u);
        EXPECT_EQ(row.find_first_not_of(U' ', arrow + 1), border);
    }
    EXPECT_TRUE(found);
}

// Answers each frame with the next scripted event, computed from what that frame shows.
class ScriptedFrameBackend final : public oui::Backend
{
  public:
    using Step = std::function<oui::Event(const oui::CellBuffer&)>;

    explicit ScriptedFrameBackend(std::vector<Step> steps, oui::Size size = {100, 30})
        : m_steps{std::move(steps)}, m_size{size}
    {
    }

    [[nodiscard]] oui::Size size() const override { return m_size; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_frames.size() <= m_next_step || m_next_step >= m_steps.size())
        {
            return std::nullopt;
        }
        const auto& step{m_steps.at(m_next_step)};
        ++ m_next_step;
        return step(m_frames.back());
    }

    void present(const oui::CellBuffer& buffer) override { m_frames.push_back(buffer); }

    [[nodiscard]] const std::vector<oui::CellBuffer>& frames() const { return m_frames; }

  private:
    std::vector<Step> m_steps;
    oui::Size m_size;
    std::size_t m_next_step{};
    std::vector<oui::CellBuffer> m_frames;
};

std::u32string row_text(const oui::CellBuffer& frame, int y)
{
    std::u32string row;
    for(auto x{0}; x < frame.width(); ++ x)
    {
        row += frame.cell_at(x, y).glyph;
    }
    return row;
}

// A press on the header cell that shows `title`, or with `release` the button let go there.
oui::Event click_header(const oui::CellBuffer& frame, std::u32string_view title, bool release = false)
{
    for(auto y{0}; y < frame.height(); ++ y)
    {
        const auto row{row_text(frame, y)};
        if(row.find(U"Name") != std::u32string::npos && row.find(U"Type") != std::u32string::npos)
        {
            oui::MouseEvent mouse{static_cast<int>(row.find(title)), y, !release};
            mouse.released = release;
            return mouse;
        }
    }
    return oui::KeyEvent{oui::Key::escape};
}

// The listed names, top to bottom, among those given.
std::vector<std::string> listed(const oui::CellBuffer& frame, const std::vector<std::u32string>& names)
{
    std::vector<std::pair<int, std::string>> found;
    for(auto y{0}; y < frame.height(); ++ y)
    {
        const auto row{row_text(frame, y)};
        for(const auto& name : names)
        {
            if(row.find(name) != std::u32string::npos)
            {
                found.emplace_back(y, std::string(name.begin(), name.end()));
            }
        }
    }
    std::sort(found.begin(), found.end());
    std::vector<std::string> result;
    for(const auto& entry : found)
    {
        result.push_back(entry.second);
    }
    return result;
}

// Where the resize grip of the dialog sits in this frame.
oui::Point grip(const oui::CellBuffer& frame)
{
    for(auto y{frame.height() - 1}; y >= 0; -- y)
    {
        const auto row{row_text(frame, y)};
        const auto corner{row.find(U"─┘")};
        if(corner != std::u32string::npos)
        {
            return {static_cast<int>(corner) + 1, y};
        }
    }
    return {-1, -1};
}

TEST(OuiFilePickerDialogTests, TheDialogResizesFromItsGrip)
{
    const auto press_grip{[](const oui::CellBuffer& frame) -> oui::Event
                          {
                              const auto corner{grip(frame)};
                              return oui::MouseEvent{corner.x, corner.y, true};
                          }};
    const auto drag_grip{[](const oui::CellBuffer& frame) -> oui::Event
                         {
                             const auto corner{grip(frame)};
                             return oui::MouseEvent{corner.x + 10, corner.y + 3, false, 0, true};
                         }};
    const auto release{[](const oui::CellBuffer& frame) -> oui::Event
                       {
                           const auto corner{grip(frame)};
                           return oui::MouseEvent{corner.x, corner.y, false, 0, false, true};
                       }};
    const auto escape{[](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; }};
    auto backend{std::make_unique<ScriptedFrameBackend>(
        std::vector<ScriptedFrameBackend::Step>{press_grip, drag_grip, release, escape})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, open_options(std::filesystem::temp_directory_path())));

    const auto& frames{recorder.frames()};
    ASSERT_GE(frames.size(), 3u);
    const auto before{grip(frames.front())};
    const auto after{grip(frames.back())};
    EXPECT_EQ(dialog_width(frames.front(), U"Open File"), 72);
    EXPECT_EQ(dialog_width(frames.back(), U"Open File"), 82);
    EXPECT_EQ(after.y, before.y + 3);
}

TEST(OuiFilePickerDialogTests, TheFileListGrowsWithTheDialog)
{
    const auto grip_at{
        [](const oui::CellBuffer& frame, int down, bool pressed, bool dragging, bool released) -> oui::Event
        {
            const auto corner{grip(frame)};
            return oui::MouseEvent{corner.x, corner.y + down, pressed, 0, dragging, released};
        }};
    auto backend{std::make_unique<ScriptedFrameBackend>(
        std::vector<ScriptedFrameBackend::Step>{
            [&grip_at](const oui::CellBuffer& frame) { return grip_at(frame, 0, true, false, false); },
            [&grip_at](const oui::CellBuffer& frame) { return grip_at(frame, 24, false, true, false); },
            [&grip_at](const oui::CellBuffer& frame) { return grip_at(frame, 0, false, false, true); },
            [](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; }},
        oui::Size{100, 60})};
    const auto& recorder{*backend};
    // The list in a colour of its own, to tell it from the dialog around it.
    oui::Theme theme{};
    theme.table_background = {1, 2, 3};
    oui::Application application{std::move(backend), theme};
    application.set_content(oui::window("Background"));
    // A folder with little in it, so the list is not tall for having much to show.
    const auto folder{std::filesystem::temp_directory_path() / "olagarro_file_list_grows"};
    std::filesystem::create_directories(folder);
    std::ofstream{folder / "only.txt"} << "x";

    static_cast<void>(oui::file_dialog(application, open_options(folder)));
    std::filesystem::remove_all(folder);

    // Much taller, the list reaches down to the blank row above the file name, whatever the folder holds.
    const auto& frame{recorder.frames().back()};
    const auto find{[&frame](std::u32string_view text) -> std::pair<int, int>
                    {
                        for(auto y{0}; y < frame.height(); ++ y)
                        {
                            std::u32string row;
                            for(auto x{0}; x < frame.width(); ++ x)
                            {
                                row += frame.cell_at(x, y).glyph;
                            }
                            if(const auto at{row.find(text)}; at != std::u32string::npos)
                            {
                                return {static_cast<int>(at), y};
                            }
                        }
                        return {-1, -1};
                    }};
    const auto [header_x, header_y]{find(U"Name")};
    const auto [name_x, name_y]{find(U"File name")};
    ASSERT_GE(header_y, 0);
    ASSERT_GT(name_y, header_y);
    EXPECT_EQ(frame.cell_at(header_x, name_y - 2).background, theme.table_background);
}

TEST(OuiFilePickerDialogTests, SortsTheFileListByTheClickedHeader)
{
    const auto folder{std::filesystem::temp_directory_path() / "oui_file_picker_sort_test"};
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "zfolder");
    std::ofstream{folder / "alpha.txt"} << std::string(3000, 'a');
    std::ofstream{folder / "beta.cpp"} << std::string(10, 'b');
    std::ofstream{folder / "gamma.md"} << std::string(90000, 'c');

    const auto escape{[](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; }};
    auto backend{std::make_unique<ScriptedFrameBackend>(std::vector<ScriptedFrameBackend::Step>{
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Size"); },
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Size", true); },
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Size"); },
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Size", true); },
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Type"); },
        [](const oui::CellBuffer& frame) { return click_header(frame, U"Type", true); }, escape})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, open_options(folder)));

    const std::vector<std::u32string> names{U"zfolder", U"alpha.txt", U"beta.cpp", U"gamma.md"};
    const auto& frames{recorder.frames()};
    ASSERT_GE(frames.size(), 7u);
    // Folders stay first in every order; a header sorts when let go, so every other frame shows it held down.
    EXPECT_EQ(listed(frames.at(0), names), (std::vector<std::string>{"zfolder", "alpha.txt", "beta.cpp", "gamma.md"}));
    EXPECT_EQ(listed(frames.at(1), names), (std::vector<std::string>{"zfolder", "alpha.txt", "beta.cpp", "gamma.md"}));
    EXPECT_EQ(listed(frames.at(2), names), (std::vector<std::string>{"zfolder", "beta.cpp", "alpha.txt", "gamma.md"}));
    EXPECT_EQ(listed(frames.at(4), names), (std::vector<std::string>{"zfolder", "gamma.md", "alpha.txt", "beta.cpp"}));
    EXPECT_EQ(listed(frames.at(6), names), (std::vector<std::string>{"zfolder", "beta.cpp", "gamma.md", "alpha.txt"}));
    std::filesystem::remove_all(folder);
}

// A press on the first cell showing `text`, optionally a double click.
oui::Event click_on(const oui::CellBuffer& frame, std::u32string_view text)
{
    for(auto y{0}; y < frame.height(); ++ y)
    {
        const auto column{row_text(frame, y).find(text)};
        if(column != std::u32string::npos)
        {
            return oui::MouseEvent{static_cast<int>(column), y, true};
        }
    }
    return oui::KeyEvent{oui::Key::escape};
}

bool shows(const oui::CellBuffer& frame, std::u32string_view text)
{
    for(auto y{0}; y < frame.height(); ++ y)
    {
        if(row_text(frame, y).find(text) != std::u32string::npos)
        {
            return true;
        }
    }
    return false;
}

class OuiFileDialogFolderTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_folder = std::filesystem::temp_directory_path() /
                   (std::string{"oui_file_dialog_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(m_folder);
        std::filesystem::create_directories(m_folder / "inner");
        std::ofstream{m_folder / "inner" / "notes.txt"} << "x";
        std::ofstream{m_folder / "inner" / "main.cpp"} << "x";
        std::ofstream{m_folder / "existing.txt"} << "x";
    }
    void TearDown() override { std::filesystem::remove_all(m_folder); }

    std::filesystem::path m_folder;
};

TEST_F(OuiFileDialogFolderTest, ThePickedFilterSurvivesChangingFolder)
{
    oui::FileDialogOptions options;
    options.directory = m_folder;
    options.filters = {{"Everything", "*"}, {"Text only", "*.txt"}};
    const auto press{[](std::u32string_view text)
                     { return [text](const oui::CellBuffer& frame) { return click_on(frame, text); }; }};
    const auto release{[](const oui::CellBuffer&) -> oui::Event
                       { return oui::MouseEvent{0, 0, false, 0, false, true}; }};
    auto backend{std::make_unique<ScriptedFrameBackend>(std::vector<ScriptedFrameBackend::Step>{
        press(U"Everything"), release, press(U"Text only"), release, press(U"inner"), release, press(U"inner"), release,
        [](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; }})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, options));

    const auto& last{recorder.frames().back()};
    EXPECT_TRUE(shows(last, U"notes.txt"));
    EXPECT_FALSE(shows(last, U"main.cpp")) << "the Text only filter still applies inside the new folder";
    EXPECT_TRUE(shows(last, U"Text only"));
}

TEST_F(OuiFileDialogFolderTest, EnterOnAFolderOpensIt)
{
    // Opening a folder replaces the entries, the folder's path among them; the dialog must not keep using it.
    const auto key{[](oui::Key value)
                   { return [value](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{value}; }; }};
    auto backend{std::make_unique<ScriptedFrameBackend>(
        std::vector<ScriptedFrameBackend::Step>{key(oui::Key::down), key(oui::Key::enter), key(oui::Key::escape)})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    static_cast<void>(oui::file_dialog(application, open_options(m_folder)));

    const auto& last{recorder.frames().back()};
    EXPECT_TRUE(shows(last, U"notes.txt"));
    EXPECT_FALSE(shows(last, U"existing.txt"));
}

TEST_F(OuiFileDialogFolderTest, TitleAndTextsComeFromTheOptions)
{
    oui::FileDialogOptions options;
    options.directory = m_folder;
    options.title = "Abrir fichero";
    options.texts.open = "Abrir";
    options.texts.cancel = "Cancelar";
    options.texts.file_name = "Nombre:";
    auto backend{std::make_unique<ScriptedFrameBackend>(std::vector<ScriptedFrameBackend::Step>{
        [](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; }})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    EXPECT_FALSE(oui::file_dialog(application, options));

    const auto& first{recorder.frames().front()};
    for(const auto* const text : {U"Abrir fichero", U"Abrir", U"Cancelar", U"Nombre:"})
    {
        EXPECT_TRUE(shows(first, text));
    }
    EXPECT_FALSE(shows(first, U"Open File"));
}

TEST_F(OuiFileDialogFolderTest, SaveStartsFromTheGivenNameAndConfirmsAnOverwrite)
{
    oui::FileDialogOptions options;
    options.mode = oui::FileDialogMode::save_file;
    options.directory = m_folder;
    options.file_name = "existing.txt";
    const auto enter{[](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::enter}; }};
    auto backend{std::make_unique<ScriptedFrameBackend>(std::vector<ScriptedFrameBackend::Step>{enter, enter})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    const auto chosen{oui::file_dialog(application, options)};

    ASSERT_TRUE(chosen);
    EXPECT_EQ(*chosen, m_folder / "existing.txt");
    ASSERT_GE(recorder.frames().size(), 2u);
    EXPECT_TRUE(shows(recorder.frames().at(1), U"Overwrite File?")) << "the first Enter only arms the overwrite";
}

TEST_F(OuiFileDialogFolderTest, SaveOffersANameWhoseStemTypingReplaces)
{
    oui::FileDialogOptions options;
    options.mode = oui::FileDialogMode::save_file;
    options.directory = m_folder;
    std::vector<ScriptedFrameBackend::Step> steps;
    for(const auto character : std::u32string{U"notes"})
    {
        steps.push_back([character](const oui::CellBuffer&) -> oui::Event { return oui::TextEvent{character}; });
    }
    steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::enter}; });
    auto backend{std::make_unique<ScriptedFrameBackend>(std::move(steps))};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    const auto chosen{oui::file_dialog(application, options)};

    ASSERT_FALSE(recorder.frames().empty());
    EXPECT_TRUE(shows(recorder.frames().front(), U"untitled.txt"));
    ASSERT_TRUE(chosen);
    EXPECT_EQ(*chosen, m_folder / "notes.txt");
}

TEST_F(OuiFileDialogFolderTest, SavingMakesAFolderAndGoesIntoIt)
{
    oui::FileDialogOptions options;
    options.mode = oui::FileDialogMode::save_file;
    options.directory = m_folder;
    std::vector<ScriptedFrameBackend::Step> steps;
    // Alt+F: New Folder, which asks its name.
    steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::TextEvent{U'f', false, true}; });
    for(const auto character : std::u32string{U"made"})
    {
        steps.push_back([character](const oui::CellBuffer&) -> oui::Event { return oui::TextEvent{character}; });
    }
    steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::enter}; });
    // In it, the name offered saved there.
    steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::enter}; });
    auto backend{std::make_unique<ScriptedFrameBackend>(std::move(steps))};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    const auto chosen{oui::file_dialog(application, options)};

    EXPECT_TRUE(std::filesystem::is_directory(m_folder / "made"));
    ASSERT_TRUE(chosen);
    EXPECT_EQ(*chosen, m_folder / "made" / "untitled.txt");
}

TEST_F(OuiFileDialogFolderTest, SaveOffersANewFileTheEndingItIsGiven)
{
    // As a description not saved yet is offered untitled.oui, and typing still replaces the name before the ending.
    oui::FileDialogOptions options;
    options.mode = oui::FileDialogMode::save_file;
    options.directory = m_folder;
    options.new_file_extension = ".oui";
    std::vector<ScriptedFrameBackend::Step> steps;
    for(const auto character : std::u32string{U"form"})
    {
        steps.push_back([character](const oui::CellBuffer&) -> oui::Event { return oui::TextEvent{character}; });
    }
    steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::enter}; });
    auto backend{std::make_unique<ScriptedFrameBackend>(std::move(steps))};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    const auto chosen{oui::file_dialog(application, options)};

    ASSERT_FALSE(recorder.frames().empty());
    EXPECT_TRUE(shows(recorder.frames().front(), U"untitled.oui"));
    ASSERT_TRUE(chosen);
    EXPECT_EQ(*chosen, m_folder / "form.oui");
}

} // namespace
