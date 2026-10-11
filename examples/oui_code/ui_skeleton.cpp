#include "ui_skeleton.h"

#include <oui/declarative.h>

#include <algorithm>
#include <array>
#include <cctype>

using namespace olagarro;

namespace
{

// How a script names the widget with the id: ui.go, or ui["save-as"] for an id a name cannot be.
[[nodiscard]] std::string script_name(std::string_view id)
{
    const auto plain{
        std::all_of(id.begin(), id.end(), [](char character)
                    { return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_'; })};
    return plain ? "ui." + std::string{id} : "ui[\"" + std::string{id} + "\"]";
}

// What a widget with an id is usually wanted for, as script lines.
void wire_script(const oui::DeclarativeNode& node, std::string& script)
{
    if(const auto id{node.property("id")})
    {
        const auto name{script_name(*id)};
        const auto& type{node.type};
        if(type == "Button")
        {
            script += name + ".onClick = () => {\n};\n";
        }
        else if(type == "LineEdit" || type == "TextEdit")
        {
            script += name + ".onChange = (text) => {\n};\n";
        }
        else if(type == "CheckButton")
        {
            script += name + ".onToggle = (checked) => {\n};\n";
        }
        else if(type == "ComboBox")
        {
            script += name + ".items = [];\n" + name + ".onSelect = (index) => {\n};\n";
        }
        else if(type == "Tabs")
        {
            script += name + ".onSelect = (index) => {\n};\n";
        }
        else if(type == "MarkdownView")
        {
            script += name + ".markdown = \"\";\n";
        }
        else if(type == "Label")
        {
            script += "// " + name + ".text = \"\";\n";
        }
    }
    for(const auto& child : node.children)
    {
        wire_script(child, script);
    }
}

// A C++ variable for the widget with the id: the id, with what a name cannot have as _, and _ after a keyword.
[[nodiscard]] std::string variable_of(std::string_view id)
{
    static constexpr std::array<std::string_view, 92> keywords{
        "alignas",     "alignof",   "and",        "and_eq",    "asm",      "auto",         "bitand",
        "bitor",       "bool",      "break",      "case",      "catch",    "char",         "char8_t",
        "char16_t",    "char32_t",  "class",      "compl",     "concept",  "const",        "consteval",
        "constexpr",   "constinit", "const_cast", "continue",  "co_await", "co_return",    "co_yield",
        "decltype",    "default",   "delete",     "do",        "double",   "dynamic_cast", "else",
        "enum",        "explicit",  "export",     "extern",    "false",    "float",        "for",
        "friend",      "goto",      "if",         "inline",    "int",      "long",         "mutable",
        "namespace",   "new",       "noexcept",   "not",       "not_eq",   "nullptr",      "operator",
        "or",          "or_eq",     "private",    "protected", "public",   "register",     "reinterpret_cast",
        "requires",    "return",    "short",      "signed",    "sizeof",   "static",       "static_assert",
        "static_cast", "struct",    "switch",     "template",  "this",     "thread_local", "throw",
        "true",        "try",       "typedef",    "typeid",    "typename", "union",        "unsigned",
        "using",       "virtual",   "void",       "volatile",  "wchar_t",  "while",        "xor",
        "xor_eq"};
    std::string variable;
    for(const auto character : id)
    {
        variable += std::isalnum(static_cast<unsigned char>(character)) != 0 ? character : '_';
    }
    if(std::find(keywords.begin(), keywords.end(), variable) != keywords.end() || variable == "ui" ||
       variable == "application" || variable == "description")
    {
        variable += '_';
    }
    return variable;
}

// What a widget with an id is usually wanted for, as C++ lines in a function's body.
void wire_cpp(const oui::DeclarativeNode& node, std::string& code)
{
    if(const auto id{node.property("id")})
    {
        const std::string quoted{"\"" + std::string{*id} + "\""};
        const auto variable{variable_of(*id)};
        const auto& type{node.type};
        const auto take{[&](std::string_view kind) {
            code += "    auto& " + variable + "{ui.get<oui::" + std::string{kind} + ">(" + quoted + ")};\n";
        }};
        if(type == "Button")
        {
            take("Button");
            code += "    " + variable + ".on_clicked([&] {});\n";
        }
        else if(type == "LineEdit")
        {
            take("LineEdit");
            code += "    " + variable + ".on_changed([&](std::string_view /*text*/) {});\n";
        }
        else if(type == "TextEdit")
        {
            take("TextEdit");
            code += "    " + variable + ".on_changed([&] {});\n";
        }
        else if(type == "CheckButton")
        {
            take("CheckButton");
            code += "    " + variable + ".on_toggled([&](bool /*checked*/) {});\n";
        }
        else if(type == "ComboBox")
        {
            take("ComboBox");
            code += "    " + variable + ".set_options({});\n";
            code += "    " + variable + ".on_selected([&](std::size_t /*index*/) {});\n";
        }
        else if(type == "Tabs")
        {
            take("TabView");
            code += "    " + variable + ".on_selection_changed([&](std::size_t /*index*/) {});\n";
        }
        else if(type == "MarkdownView")
        {
            code += "    ui.get<oui::MarkdownView>(" + quoted + ").set_markdown(\"\");\n";
        }
        else if(type == "Label")
        {
            code += "    // ui.get<oui::Label>(" + quoted + ").set_text(\"\");\n";
        }
    }
    for(const auto& child : node.children)
    {
        wire_cpp(child, code);
    }
}

} // namespace

std::string script_skeleton(std::string_view description, std::string_view file)
{
    const auto parsed{oui::parse_oui(description)};
    if(!parsed.succeeded())
    {
        return {};
    }
    std::string script;
    script += "// The window " + std::string{file} +
              " describes, found beside this script: each widget with an id is ui.<id>.\n"
              "const ui = oui.uiFromFile(\"" +
              std::string{file} + "\");\n\n";
    wire_script(*parsed.root, script);
    script += "\n";
    script += parsed.root->type == "Window" ? "oui.window(ui);\n" : "oui.window(\"Window\", ui.root);\n";
    return script;
}

std::string cpp_skeleton(std::string_view description, std::string_view file)
{
    const auto parsed{oui::parse_oui(description)};
    if(!parsed.succeeded())
    {
        return {};
    }
    // Named after the file: show_rename_files and embedded_rename_files_ui for rename_files.oui.
    const auto name{variable_of(file.substr(0, file.rfind('.')))};
    const auto embedded{"embedded_" + name + "_ui"};
    const auto window{parsed.root->type == "Window"};
    std::string code;
    code += std::string{"#include <oui/oui.h>\n\n#include <cstddef>\n#include <string_view>\n"} +
            (window ? "" : "#include <utility>\n") + "#include <vector>\n\nusing namespace olagarro;\n\n";
    code += "// " + std::string{file} +
            ", built into the program by CMake, in CMakeLists.txt:\n//     olagarro_embed_files(<target> FUNCTION " +
            embedded + " FILES <its folder>/" + std::string{file} + ")\nstd::vector<std::string_view> " + embedded +
            "();\n\n";
    code += "// Shows the window " + std::string{file} + " describes and waits for it to close.\nvoid show_" + name +
            "(oui::Application& application)\n{\n" +
            // Texts to translate need the context= they belong to; without one they show as written, as a script's.
            (parsed.root->has("context")
                 ? "    auto ui{oui::build_ui(" + embedded + "().front())};\n"
                 : "    // As written: with a context= in the description, its texts are translated.\n"
                   "    auto ui{oui::build_ui(" +
                       embedded + "().front(), oui::UiTexts::as_written)};\n");
    wire_cpp(*parsed.root, code);
    if(window)
    {
        code += "    static_cast<void>(application.show_modal(ui.take_window()));\n";
    }
    else
    {
        code += "    auto window{oui::window(\"Window\")};\n    window->add(ui.take());\n"
                "    static_cast<void>(application.show_modal(std::move(window)));\n";
    }
    code += "}\n";
    // Or the file read as the program runs, for whoever would rather not build it in.
    code += "\n// Or " + std::string{file} +
            " read from disk as the program runs, rather than built in: without the CMake line, this,\n"
            "// with the path to it from where the program is started.\n"
            "// std::vector<std::string_view> " +
            embedded +
            "()\n"
            "// {\n"
            "//     static const auto text{oui::read_file(\"" +
            std::string{file} +
            "\").value_or(\"\")};\n"
            "//     return {text};\n"
            "// }\n";
    return code;
}
