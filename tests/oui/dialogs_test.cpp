#include <oui/application.h>
#include <oui/backend.h>
#include <oui/dialogs.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using namespace olagarro;

namespace
{

// Replays events one per frame, after the first frame, then keeps quiet.
class ReplayBackend final : public oui::Backend
{
  public:
    explicit ReplayBackend(std::vector<oui::Event> events) : m_events{std::move(events)} {}

    [[nodiscard]] oui::Size size() const override { return {60, 20}; }

    std::optional<oui::Event> poll_event() override
    {
        if(m_frames <= m_next || m_next >= m_events.size())
        {
            return std::nullopt;
        }
        return m_events.at(m_next++);
    }

    void present(const oui::CellBuffer& buffer) override
    {
        ++ m_frames;
        m_last = buffer;
        m_all.push_back(buffer);
    }

    // Whether some frame showed `text` on one row, such as a dialog already closed.
    [[nodiscard]] bool showed(std::u32string_view text) const
    {
        for(const auto& frame : m_all)
        {
            for(auto y{0}; y < frame.height(); ++ y)
            {
                std::u32string row;
                for(auto x{0}; x < frame.width(); ++ x)
                {
                    row += frame.cell_at(x, y).glyph;
                }
                if(row.find(text) != std::u32string::npos)
                {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool shows(std::u32string_view text) const
    {
        for(auto y{0}; y < m_last.height(); ++ y)
        {
            std::u32string row;
            for(auto x{0}; x < m_last.width(); ++ x)
            {
                row += m_last.cell_at(x, y).glyph;
            }
            if(row.find(text) != std::u32string::npos)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const std::vector<oui::CellBuffer>& frames() const { return m_all; }

  private:
    std::vector<oui::Event> m_events;
    std::size_t m_next{};
    std::size_t m_frames{};
    oui::CellBuffer m_last{1, 1};
    std::vector<oui::CellBuffer> m_all;
};

oui::KeyEvent key(oui::Key value)
{
    return oui::KeyEvent{value};
}

std::vector<oui::Event> typing(std::u32string_view text)
{
    std::vector<oui::Event> events;
    for(const auto character : text)
    {
        events.emplace_back(oui::TextEvent{character});
    }
    return events;
}

TEST(OuiDialogTests, MessageBoxReturnsTheButtonPressed)
{
    auto backend{std::make_unique<ReplayBackend>(std::vector<oui::Event>{key(oui::Key::tab), key(oui::Key::enter)})};
    const auto& replay{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    const auto answer{oui::message_box(application, "Unsaved Changes", "Save before closing?\nIt has edits.",
                                       {"Save", "Discard", "Cancel"})};
    EXPECT_EQ(answer, std::optional<std::size_t>{1});
    EXPECT_TRUE(replay.shows(U"It has edits."));
}

TEST(OuiDialogTests, AMessageLineWiderThanTheScreenWrapsAndKeepsItsEnd)
{
    auto backend{std::make_unique<ReplayBackend>(std::vector<oui::Event>{key(oui::Key::escape)})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    // Wider than the 60-column screen: the name at the end must still show.
    const std::string path{"/home/someone/projects/a/rather/deep/folder/structure/that/goes/on/target_file.txt"};
    static_cast<void>(oui::message_box(application, "Delete", "Delete this file?\n" + path, {"Delete"}));
    EXPECT_TRUE(recorder.showed(U"target_file.txt"));
}

TEST(OuiDialogTests, AMessageBoxCentresItsButtons)
{
    auto backend{std::make_unique<ReplayBackend>(std::vector<oui::Event>{key(oui::Key::escape)})};
    const auto& recorder{*backend};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));
    static_cast<void>(
        oui::message_box(application, "oui_run", "Publish v0.1.7 on the minipc and GitHub?", {"Yes", "No"}));
    ASSERT_FALSE(recorder.frames().empty());
    const auto& frame{recorder.frames().front()};
    for(int y{}; y < frame.height(); ++ y)
    {
        std::u32string row;
        for(int x{}; x < frame.width(); ++ x)
        {
            row += frame.cell_at(x, y).glyph;
        }
        const auto yes{row.find(U"Yes")};
        if(yes == std::u32string::npos)
        {
            continue;
        }
        // The dialog's frame on that row, either side, and the room between it and the buttons.
        const auto left{row.rfind(U'\u2551', yes)};
        const auto no_end{row.find(U"No", yes) + 2};
        const auto right{row.find(U'\u2551', no_end)};
        ASSERT_NE(left, std::u32string::npos);
        ASSERT_NE(right, std::u32string::npos);
        const auto before{static_cast<int>(yes - left)};
        const auto after{static_cast<int>(right - no_end)};
        EXPECT_LE(std::abs(before - after), 3) << "before " << before << ", after " << after;
        return;
    }
    ADD_FAILURE() << "no row with the buttons";
}

TEST(OuiDialogTests, MessageBoxDismissedWithEscapeAnswersNothing)
{
    oui::Application application{std::make_unique<ReplayBackend>(std::vector<oui::Event>{key(oui::Key::escape)})};
    application.set_content(oui::window("Background"));
    EXPECT_FALSE(oui::message_box(application, "Error", "Something failed."));
}

TEST(OuiDialogTests, PromptReturnsTheTextOnlyOnceItIsValid)
{
    auto events{typing(U"x")};
    events.emplace_back(key(oui::Key::enter));
    events.emplace_back(key(oui::Key::backspace));
    for(const auto& event : typing(U"12"))
    {
        events.push_back(event);
    }
    events.emplace_back(key(oui::Key::enter));
    auto backend{std::make_unique<ReplayBackend>(events)};
    oui::Application application{std::move(backend)};
    application.set_content(oui::window("Background"));

    oui::PromptOptions options;
    options.title = "Go To Line";
    options.label = "Line number:";
    options.validate = [](std::string_view text) -> std::optional<std::string>
    {
        if(text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos)
        {
            return "Type a number.";
        }
        return std::nullopt;
    };
    EXPECT_EQ(oui::prompt(application, options), std::optional<std::string>{"12"});
}

TEST(OuiDialogTests, PromptStartsWithItsTextAndCancelsWithEscape)
{
    oui::Application application{std::make_unique<ReplayBackend>(std::vector<oui::Event>{key(oui::Key::escape)})};
    application.set_content(oui::window("Background"));
    oui::PromptOptions options;
    options.text = "initial";
    EXPECT_FALSE(oui::prompt(application, options));
}

TEST(OuiDialogTests, PromptTextStartsSelectedSoTypingReplacesIt)
{
    auto events{typing(U"new")};
    events.emplace_back(key(oui::Key::enter));
    oui::Application application{std::make_unique<ReplayBackend>(events)};
    application.set_content(oui::window("Background"));
    oui::PromptOptions options;
    options.text = "old.txt";
    // Only the name: the extension stays, as a file manager renames.
    options.selection = std::pair<std::size_t, std::size_t>{0, 3};
    EXPECT_EQ(oui::prompt(application, options), std::optional<std::string>{"new.txt"});

    auto replace_all{typing(U"other")};
    replace_all.emplace_back(key(oui::Key::enter));
    oui::Application second{std::make_unique<ReplayBackend>(replace_all)};
    second.set_content(oui::window("Background"));
    oui::PromptOptions whole;
    whole.text = "old.txt";
    EXPECT_EQ(oui::prompt(second, whole), std::optional<std::string>{"other"});
}

} // namespace
