#include <oui/application.h>
#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/command_palette.h>
#include <oui/quick_pick.h>
#include <oui/scripted_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <functional>

#include <string>
#include <string_view>
#include <vector>

using namespace olagarro;

namespace
{

// Types the scripted keys one per frame, then closes the palette, keeping every frame it was shown.
class TypingBackend final : public oui::Backend
{
  public:
    explicit TypingBackend(std::vector<oui::Event> events) : m_events{std::move(events)}
    {
        m_events.push_back(oui::KeyEvent{oui::Key::escape});
    }

    [[nodiscard]] oui::Size size() const override { return {100, 30}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_frames.size() <= m_next || m_next >= m_events.size())
        {
            return std::nullopt;
        }
        return m_events.at(m_next++);
    }

    void present(const oui::CellBuffer& buffer) override { m_frames.push_back(buffer); }

    [[nodiscard]] const std::vector<oui::CellBuffer>& frames() const { return m_frames; }

  private:
    std::vector<oui::Event> m_events;
    std::size_t m_next{};
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

// Width and height of the frame whose top border carries `title`, measured on its bottom border.
oui::Size dialog_size(const oui::CellBuffer& frame, std::u32string_view title)
{
    for(auto y{0}; y < frame.height(); ++ y)
    {
        const auto top{row_text(frame, y)};
        const auto title_at{top.find(title)};
        if(title_at == std::u32string::npos)
        {
            continue;
        }
        const auto left{top.find_last_of(U"┌╔", title_at)};
        for(auto bottom_y{y + 1}; left != std::u32string::npos && bottom_y < frame.height(); ++ bottom_y)
        {
            const auto bottom{row_text(frame, bottom_y)};
            if(bottom.at(left) == U'└' || bottom.at(left) == U'╚')
            {
                const auto right{bottom.find_first_of(U"┘╝", left)};
                return {right == std::u32string::npos ? 0.0 : static_cast<double>(right - left + 1),
                        static_cast<double>(bottom_y - y + 1)};
            }
        }
    }
    return {};
}

// Whether any row of the frame contains `text`.
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

TEST(OuiCommandPaletteTests, ShowsTheStateOfToggleCommandsAfterTheirTitle)
{
    auto backend{std::make_unique<TypingBackend>(std::vector<oui::Event>{})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    application.commands().register_command("view.word_wrap", "&Word Wrap", [] {});
    application.commands().set_checked("view.word_wrap", true);
    application.commands().register_command("view.toggle_menu", "Toggle Menu", [] {});
    application.commands().set_checked("view.toggle_menu", false);
    application.commands().bind("view.toggle_menu", {U'm', true});

    oui::show_command_palette(application);

    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    // On the title's row, in the column of details.
    const auto row_with{[&frame](std::u32string_view title)
                        {
                            for(auto y{0}; y < frame.height(); ++ y)
                            {
                                if(const auto text{row_text(frame, y)}; text.find(title) != std::u32string::npos)
                                {
                                    return text;
                                }
                            }
                            return std::u32string{};
                        }};
    EXPECT_NE(row_with(U" Word Wrap ").find(U" (on)"), std::u32string::npos);
    EXPECT_NE(row_with(U" Toggle Menu ").find(U" Ctrl+M  (off)"), std::u32string::npos);
    EXPECT_FALSE(shows(frame, U"[■]"));
    EXPECT_FALSE(shows(frame, U"[ ]"));
    EXPECT_FALSE(shows(frame, U"&")) << "mnemonic marks are for menus, not the palette";
}

TEST(OuiCommandPaletteTests, KeepsItsSizeWhileTheFilterNarrowsTheList)
{
    auto backend{std::make_unique<TypingBackend>(std::vector<oui::Event>{oui::TextEvent{U'q'}})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    for(const auto* const title : {"Quit", "Open File", "Save", "Save As", "Find", "Replace", "Go To Line"})
    {
        application.commands().register_command(title, title, [] {});
    }

    oui::show_command_palette(application);

    const auto& frames{recorder.frames()};
    ASSERT_GE(frames.size(), 2u);
    const auto before{dialog_size(frames.at(0), U"Command Palette")};
    const auto after{dialog_size(frames.at(1), U"Command Palette")};
    EXPECT_GE(before.width, 56);
    EXPECT_EQ(after.width, before.width);
    EXPECT_EQ(after.height, before.height);
}

// Answers each frame with an event computed from what it shows, then closes the palette.
class FrameScriptBackend final : public oui::Backend
{
  public:
    using Step = std::function<oui::Event(const oui::CellBuffer&)>;

    explicit FrameScriptBackend(std::vector<Step> steps) : m_steps{std::move(steps)}
    {
        m_steps.push_back([](const oui::CellBuffer&) -> oui::Event { return oui::KeyEvent{oui::Key::escape}; });
    }

    [[nodiscard]] oui::Size size() const override { return {100, 30}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_frames.size() <= m_next || m_next >= m_steps.size())
        {
            return std::nullopt;
        }
        return m_steps.at(m_next++)(m_frames.back());
    }

    void present(const oui::CellBuffer& buffer) override { m_frames.push_back(buffer); }

    [[nodiscard]] const std::vector<oui::CellBuffer>& frames() const { return m_frames; }

  private:
    std::vector<Step> m_steps;
    std::size_t m_next{};
    std::vector<oui::CellBuffer> m_frames;
};

// The resize grip: the single-line corner a resizable window draws.
oui::Point grip(const oui::CellBuffer& frame)
{
    for(auto y{frame.height() - 1}; y >= 0; -- y)
    {
        const auto corner{row_text(frame, y).find(U"─┘")};
        if(corner != std::u32string::npos)
        {
            return {static_cast<double>(corner) + 1.0, static_cast<double>(y)};
        }
    }
    return {-1, -1};
}

TEST(OuiCommandPaletteTests, ResizesFromItsGrip)
{
    const auto at_grip{[](int dx, int dy, bool press, bool drag, bool release)
                       {
                           return [=](const oui::CellBuffer& frame) -> oui::Event
                           {
                               const auto corner{grip(frame)};
                               return oui::MouseEvent{corner.x + dx, corner.y + dy, press, 0, drag, release};
                           };
                       }};
    auto backend{std::make_unique<FrameScriptBackend>(std::vector<FrameScriptBackend::Step>{
        at_grip(0, 0, true, false, false), at_grip(-12, -4, false, true, false), at_grip(0, 0, false, false, true)})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    for(const auto* const title : {"Quit", "Open File", "Save", "Save As", "Find", "Replace", "Go To Line"})
    {
        application.commands().register_command(title, title, [] {});
    }

    oui::show_command_palette(application);

    const auto& frames{recorder.frames()};
    ASSERT_GE(frames.size(), 3u);
    const auto before{dialog_size(frames.front(), U"Command Palette")};
    const auto after{dialog_size(frames.back(), U"Command Palette")};
    ASSERT_GT(before.width, 0);
    EXPECT_EQ(after.width, before.width - 12);
    EXPECT_EQ(after.height, before.height - 4);
}

} // namespace

TEST(OuiCommandPaletteTests, EnterInTheFilterRunsTheFirstCommandLeft)
{
    std::vector<oui::Event> events;
    for(const auto character : std::u32string{U"save a"})
    {
        events.emplace_back(oui::TextEvent{character});
    }
    events.emplace_back(oui::KeyEvent{oui::Key::enter});
    // When Enter finds nothing to pick, Escape closes the dialog.
    events.emplace_back(oui::KeyEvent{oui::Key::escape});
    oui::Application application{oui::scripted_backend(std::move(events))};
    application.set_content(oui::window("Background"));
    std::string ran;
    for(const auto* const title : {"Save", "Save As", "Quit"})
    {
        application.commands().register_command(title, title, [&ran, title] { ran = title; });
    }
    oui::show_command_palette(application);
    // The command runs once the palette has closed.
    application.run_frame();
    EXPECT_EQ(ran, "Save As");
}

// Types `filter` and Enter into a quick pick of `items`, and returns the picked label.
std::optional<std::string> pick_by_typing(const std::vector<std::string>& labels, const std::u32string& filter)
{
    std::vector<oui::Event> events;
    for(const auto character : filter)
    {
        events.emplace_back(oui::TextEvent{character});
    }
    events.emplace_back(oui::KeyEvent{oui::Key::enter});
    // When Enter finds nothing to pick, Escape closes the dialog.
    events.emplace_back(oui::KeyEvent{oui::Key::escape});
    oui::Application application{oui::scripted_backend(std::move(events))};
    application.set_content(oui::window("Background"));
    std::vector<oui::QuickPickItem> items;
    for(const auto& label : labels)
    {
        items.push_back({label, {}});
    }
    const auto picked{oui::quick_pick(application, items, {})};
    return picked ? std::optional<std::string>{labels.at(*picked)} : std::nullopt;
}

TEST(OuiQuickPickTests, ItsListIsInTheTablesColoursTextAndAll)
{
    // As a dark desktop theme has them: a table dark with light text, a list's text dark.
    oui::Theme theme;
    theme.table_background = {30, 30, 40};
    theme.table_foreground = {220, 220, 220};
    theme.list_foreground = {0, 0, 0};
    auto backend{std::make_unique<TypingBackend>(std::vector<oui::Event>{})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_theme(theme);
    application.set_content(oui::window("Background"));
    static_cast<void>(oui::quick_pick(application, {{"first", {}}, {"secondentry", {}}}, {}));

    ASSERT_FALSE(screen->frames().empty());
    const auto& frame{screen->frames().front()};
    auto found{false};
    for(int y{}; y < frame.height() && !found; ++ y)
    {
        // The second entry: the first is selected.
        if(const auto x{row_text(frame, y).find(U"secondentry")}; x != std::u32string::npos)
        {
            found = true;
            EXPECT_EQ(frame.cell_at(static_cast<int>(x), y).background, theme.table_background);
            EXPECT_EQ(frame.cell_at(static_cast<int>(x), y).foreground, theme.table_foreground);
        }
    }
    EXPECT_TRUE(found);
}

TEST(OuiQuickPickTests, DetailsLineUpInAColumnOfTheirOwn)
{
    // As a menu's shortcuts: whatever the length of the label before it, every detail starts at the same place.
    auto backend{std::make_unique<TypingBackend>(std::vector<oui::Event>{})};
    auto* const screen{backend.get()};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    static_cast<void>(
        oui::quick_pick(application, {{"Zoom", "Ctrl++"}, {"Command Palette", "Ctrl+Shift+P"}, {"Sort", {}}}, {}));

    ASSERT_FALSE(screen->frames().empty());
    const auto& frame{screen->frames().front()};
    std::vector<std::size_t> starts;
    for(int y{}; y < frame.height(); ++ y)
    {
        const auto text{row_text(frame, y)};
        if(const auto at{text.find(U"Ctrl+")}; at != std::u32string::npos)
        {
            starts.push_back(at);
        }
    }
    ASSERT_EQ(starts.size(), 2u);
    EXPECT_EQ(starts[0], starts[1]);
}

TEST(OuiQuickPickTests, AWildcardMatchesTheFileNameAsAShellDoesOrWithASlashThePath)
{
    const std::vector<std::string> files{"docs/hola.txt", "src/hola_mundo.h", "lib/x.h",
                                         "src/adios.h",   "web/page.html",    "src/text_edit.cpp"};
    EXPECT_EQ(pick_by_typing(files, U"hola*.h"), std::optional<std::string>{"src/hola_mundo.h"});
    EXPECT_EQ(pick_by_typing(files, U"lib/*.h"), std::optional<std::string>{"lib/x.h"});
    // As in a shell, the pattern covers the whole name: "edit*" wants names starting with "edit".
    EXPECT_EQ(pick_by_typing(files, U"edit*"), std::nullopt);
    EXPECT_EQ(pick_by_typing(files, U"*edit*"), std::optional<std::string>{"src/text_edit.cpp"});
    EXPECT_EQ(pick_by_typing(files, U"*.htm"), std::nullopt);
    // Fuzzy without wildcards: the file name counts most.
    EXPECT_EQ(pick_by_typing(files, U"adh"), std::optional<std::string>{"src/adios.h"});
}
