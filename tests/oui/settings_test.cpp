#include <oui/settings.h>
#include <oui/text_edit.h>
#include <oui/cell_buffer.h>
#include <oui/theme.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>

using namespace olagarro;

namespace
{

// The settings of the test running, in a file of its own: CTest runs tests side by side, and one that shared the file
// with another found it removed, or written by the other, between saving it and loading it back.
class ScopedSettingsFile final
{
  public:
    ScopedSettingsFile()
        : m_name{std::string{"oui_settings_tests_"} +
                 ::testing::UnitTest::GetInstance()->current_test_info()->name()},
          m_settings{oui::SettingsLocation::next_to_executable, m_name}
    {
        remove_file();
    }

    ~ScopedSettingsFile() { remove_file(); }

    ScopedSettingsFile(const ScopedSettingsFile&) = delete;
    ScopedSettingsFile& operator=(const ScopedSettingsFile&) = delete;

    [[nodiscard]] oui::Settings& get() { return m_settings; }
    [[nodiscard]] const std::string& name() const { return m_name; }

  private:
    void remove_file()
    {
        std::error_code ec;
        std::filesystem::remove(m_settings.file_path(), ec);
    }

    std::string m_name;
    oui::Settings m_settings;
};

TEST(OuiSettingsTests, CanLiveAtAPathTheApplicationChooses)
{
    const auto path{std::filesystem::temp_directory_path() / "oui_settings_explicit_path.ini"};
    std::filesystem::remove(path);
    {
        oui::Settings settings{path};
        settings.set_value("key", "value");
        EXPECT_TRUE(settings.save());
    }
    oui::Settings reloaded{path};
    EXPECT_TRUE(reloaded.load());
    EXPECT_EQ(reloaded.value("key"), "value");
    EXPECT_EQ(reloaded.file_path(), path);
    std::filesystem::remove(path);
}

} // namespace

// A program unpacked where it can write keeps its settings beside it, leaving nothing behind it found that out with.
TEST(OuiSettingsTests, APortableProgramKeepsItsSettingsBesideItWhereItCanWrite)
{
    const oui::Settings beside{oui::SettingsLocation::next_to_executable, "oui_settings_tests"};
    const oui::Settings portable{oui::SettingsLocation::portable, "oui_settings_tests"};
    EXPECT_EQ(portable.file_path(), beside.file_path());
    std::error_code ec;
    EXPECT_FALSE(std::filesystem::exists(beside.file_path().parent_path() / ".oui_settings_tests.write_test", ec));
}

TEST(OuiSettingsTests, RoundTripsIntegers)
{
    ScopedSettingsFile scope;
    auto& settings{scope.get()};

    settings.set_int_value("tab_space_count", 2);
    EXPECT_EQ(settings.int_value("tab_space_count", 4), 2);

    settings.set_int_value("single_digit", 1);
    EXPECT_EQ(settings.int_value("single_digit", 0), 1);

    settings.set_int_value("large", 123456789);
    EXPECT_EQ(settings.int_value("large", 0), 123456789);

    settings.set_int_value("negative", -7);
    EXPECT_EQ(settings.int_value("negative", 0), -7);
}

TEST(OuiSettingsTests, PersistsIntegersAcrossSaveAndLoad)
{
    ScopedSettingsFile scope;
    scope.get().set_int_value("tab_space_count", 6);
    ASSERT_TRUE(scope.get().save());

    oui::Settings reloaded{oui::SettingsLocation::next_to_executable, scope.name()};
    ASSERT_TRUE(reloaded.load());
    EXPECT_EQ(reloaded.int_value("tab_space_count", 4), 6);
}

TEST(OuiSettingsTests, ReturnsDefaultsForMissingKeys)
{
    ScopedSettingsFile scope;
    const auto& settings{scope.get()};

    EXPECT_EQ(settings.int_value("missing", 4), 4);
    EXPECT_TRUE(settings.bool_value("missing", true));
    EXPECT_EQ(settings.value("missing", "fallback"), "fallback");
    EXPECT_TRUE(settings.list_value("missing").empty());
}

TEST(OuiSettingsTests, RoundTripsBooleansAndLists)
{
    ScopedSettingsFile scope;
    auto& settings{scope.get()};

    settings.set_bool_value("tab_use_spaces", true);
    EXPECT_TRUE(settings.bool_value("tab_use_spaces", false));
    settings.set_bool_value("tab_use_spaces", false);
    EXPECT_FALSE(settings.bool_value("tab_use_spaces", true));

    settings.set_list_value("recent_files", {"a.cpp", "b.txt"});
    const auto files{settings.list_value("recent_files")};
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(files.front(), "a.cpp");
    EXPECT_EQ(files.back(), "b.txt");
}

TEST(OuiTextEditTests, InsertsAndRemovesTheConfiguredNumberOfSpacesWithTabKeys)
{
    oui::TextEdit editor;
    editor.set_text("hi");
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::end}));
    editor.set_use_spaces(true);
    editor.set_tab_spaces(2);

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::tab}));
    EXPECT_EQ(editor.text(), "hi  ");

    editor.set_text("    indented");
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::home}));
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::shift_tab}));
    EXPECT_EQ(editor.text(), "  indented");
}

TEST(OuiTextEditTests, InsertsATabCharacterWhenSpacesAreDisabled)
{
    oui::TextEdit editor;
    editor.set_text("hi");
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::end}));
    editor.set_use_spaces(false);

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::tab}));

    EXPECT_EQ(editor.text(), "hi\t");
}

TEST(OuiTextEditTests, OutdentsOneTabCharacterAtATimeWhenSpacesAreDisabled)
{
    oui::TextEdit editor;
    editor.set_use_spaces(false);
    editor.set_text("\t\t\tcode");

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::shift_tab}));
    EXPECT_EQ(editor.text(), "\t\tcode");

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::shift_tab}));
    EXPECT_EQ(editor.text(), "\tcode");
}

TEST(OuiTextEditTests, OutdentsLeadingSpacesByOneIndentUnitInTabMode)
{
    oui::TextEdit editor;
    editor.set_use_spaces(false);
    editor.set_tab_spaces(4);
    editor.set_text("      code");

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::shift_tab}));

    EXPECT_EQ(editor.text(), "  code");
}

TEST(OuiTextEditTests, PaintsSelectedTextWithContrastingColors)
{
    const oui::Theme theme;
    oui::TextEdit editor;
    editor.set_text("ab");
    editor.arrange({0, 0, 10, 1});
    static_cast<void>(editor.handle_event(oui::MouseEvent{0, 0, true}));
    static_cast<void>(editor.handle_event(oui::MouseEvent{1, 0, false, 0, true}));
    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, theme);

    const auto& selected{buffer.cell_at(0, 0)};
    EXPECT_EQ(selected.background, theme.selection_background);
    EXPECT_EQ(selected.foreground, theme.selection_foreground);
    EXPECT_NE(selected.background, theme.editor_background);
    EXPECT_NE(selected.foreground, theme.editor_foreground);
    EXPECT_EQ(buffer.cell_at(1, 0).background, theme.editor_background);
}

TEST(OuiTextEditTests, ShiftTabOnAnEmptyDocumentIsANoop)
{
    oui::TextEdit editor;
    editor.set_use_spaces(false);

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::shift_tab}));

    EXPECT_TRUE(editor.text().empty());
}

TEST(OuiTextEditTests, InsertsSixteenSpacesWhenConfigured)
{
    oui::TextEdit editor;
    editor.set_text("hi");
    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::end}));
    editor.set_use_spaces(true);
    editor.set_tab_spaces(16);

    static_cast<void>(editor.handle_event(oui::KeyEvent{oui::Key::tab}));

    EXPECT_EQ(editor.text(), "hi                ");
}

TEST(OuiTextEditTests, RendersLiteralTabsAtAFixedWidthIgnoringTheSpacesSetting)
{
    oui::TextEdit editor;
    editor.set_text("\tx");
    editor.arrange({0, 0, 20, 1});
    editor.set_tab_spaces(16);

    static_cast<void>(editor.handle_event(oui::MouseEvent{4, 0, true}));
    EXPECT_EQ(editor.line_column(), (std::pair<std::size_t, std::size_t>{1u, 5u}));

    oui::CellBuffer buffer{10, 1};
    oui::CellBufferPainter painter{buffer};
    editor.paint(painter, {});
    EXPECT_EQ(buffer.cell_at(4, 0).glyph, U'x');
}

TEST(OuiSettingsTests, ARemovedKeyIsGoneFromTheSavedFile)
{
    const auto path{std::filesystem::temp_directory_path() / "oui_settings_removed_key.ini"};
    std::filesystem::remove(path);
    {
        oui::Settings settings{path};
        settings.set_value("kept", "1");
        settings.set_value("dropped", "2");
        settings.remove("dropped");
        EXPECT_TRUE(settings.save());
    }
    oui::Settings reloaded{path};
    EXPECT_TRUE(reloaded.load());
    EXPECT_EQ(reloaded.value("kept"), "1");
    EXPECT_EQ(reloaded.value("dropped", "absent"), "absent");
    std::filesystem::remove(path);
}

TEST(OuiSettingsTests, AValueKeepsWhatEndsItThroughSaveAndLoad)
{
    // A list's last entry can end in a tab, as a session's tab with an empty last part does: cut, the tab came back
    // with a part missing, and did not come back at all.
    ScopedSettingsFile scope;
    scope.get().set_list_value("tabs", {"one\t", "two\t"});
    ASSERT_TRUE(scope.get().save());

    oui::Settings reloaded{oui::SettingsLocation::next_to_executable, scope.name()};
    ASSERT_TRUE(reloaded.load());
    EXPECT_EQ(reloaded.list_value("tabs"), (std::vector<std::string>{"one\t", "two\t"}));
}
