#include "ui_skeleton.h"

#include <oui/ui.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include <string>

TEST(OuiCodeUiSkeletonTests, AScriptWithEachWidgetWithAnIdWiredToWhatItDoes)
{
    const std::string description{"Window \"Rename Files\"\n"
                                  "  LineEdit id=pattern\n"
                                  "  CheckButton \"&Dry run\" id=dry\n"
                                  "  ComboBox id=case\n"
                                  "  Label id=status\n"
                                  "  Row centered\n"
                                  "    Button \"&Rename\" id=go default\n"
                                  "    Button \"Cancel\"\n"};
    EXPECT_EQ(script_skeleton(description, "rename_files.oui"),
              "// The window rename_files.oui describes, found beside this script: each widget with an id is ui.<id>.\n"
              "const ui = oui.uiFromFile(\"rename_files.oui\");\n"
              "\n"
              "ui.pattern.onChange = (text) => {\n"
              "};\n"
              "ui.dry.onToggle = (checked) => {\n"
              "};\n"
              "ui.case.items = [];\n"
              "ui.case.onSelect = (index) => {\n"
              "};\n"
              "// ui.status.text = \"\";\n"
              "ui.go.onClick = () => {\n"
              "};\n"
              "\n"
              "oui.window(ui);\n");
}

TEST(OuiCodeUiSkeletonTests, ADescriptionWithNoWindowGetsOne)
{
    EXPECT_EQ(script_skeleton("Column\n  Button \"Go\" id=go\n", "go.oui"),
              "// The window go.oui describes, found beside this script: each widget with an id is ui.<id>.\n"
              "const ui = oui.uiFromFile(\"go.oui\");\n"
              "\n"
              "ui.go.onClick = () => {\n"
              "};\n"
              "\n"
              "oui.window(\"Window\", ui.root);\n");
    const auto code{cpp_skeleton("Column\n  Button \"Go\" id=go\n", "go.oui")};
    EXPECT_NE(code.find("    auto window{oui::window(\"Window\")};\n    window->add(ui.take());\n"), std::string::npos);
    EXPECT_NE(code.find("#include <utility>"), std::string::npos);
}

TEST(OuiCodeUiSkeletonTests, ADescriptionThatDoesNotReadGivesNothing)
{
    EXPECT_EQ(script_skeleton("Column\n  Label \"open\n", "a.oui"), "");
    EXPECT_EQ(cpp_skeleton("Column\n  Label \"open\n", "a.oui"), "");
}

TEST(OuiCodeUiSkeletonTests, TheSameInCppIsCodeThatBuildsWithItsFileBuiltIn)
{
    // generated_window.cpp is what comes out for rename_files.oui beside it, and is built with these tests, the file
    // built in as the comment over it says: what the generator writes compiles and links.
    const std::filesystem::path folder{std::filesystem::path{OLAGARRO_SOURCE_DIR} / "tests" / "oui_code"};
    const auto read{[&folder](const char* name)
                    {
                        std::ifstream file{folder / name, std::ios::binary};
                        std::stringstream text;
                        text << file.rdbuf();
                        return text.str();
                    }};
    EXPECT_EQ(cpp_skeleton(read("rename_files.oui"), "rename_files.oui"), read("generated_window.cpp"));
}

// The code built a description with no context= as translated, and that throws as it runs: texts to translate need
// a context. With none, the texts show as written; with one, they are translated under it.
TEST(OuiCodeUiSkeletonTests, TheCppBuildsTheDescriptionAsItsContextSays)
{
    const std::string plain{"Window \"Ask\"\n  Button \"OK\" id=ok\n"};
    EXPECT_NE(cpp_skeleton(plain, "ask.oui").find("oui::build_ui(embedded_ask_ui().front(), oui::UiTexts::as_written)"),
              std::string::npos);
    EXPECT_NO_THROW(static_cast<void>(olagarro::oui::build_ui(plain, olagarro::oui::UiTexts::as_written)));
    const std::string translated{"Window \"Ask\" context=Ask\n  Button \"OK\" id=ok\n"};
    EXPECT_NE(cpp_skeleton(translated, "ask.oui").find("oui::build_ui(embedded_ask_ui().front())"), std::string::npos);
}

TEST(OuiCodeUiSkeletonTests, AnIdThatIsNoNameIsWrittenAsOne)
{
    EXPECT_NE(script_skeleton("Row\n  Button \"Save\" id=save-as\n", "a.oui").find("ui[\"save-as\"].onClick"),
              std::string::npos);
    EXPECT_NE(cpp_skeleton("Row\n  Button \"Save\" id=save-as\n", "a.oui")
                  .find("auto& save_as{ui.get<oui::Button>(\"save-as\")};"),
              std::string::npos);
}
