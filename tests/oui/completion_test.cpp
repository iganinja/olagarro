#include <oui/completion.h>
#include <oui/file_system.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace olagarro;
using namespace olagarro::oui;

namespace
{

[[nodiscard]] std::vector<std::string> labels(const std::vector<Suggestion>& suggestions)
{
    std::vector<std::string> found;
    for(const auto& suggestion : suggestions)
    {
        found.push_back(suggestion.label);
    }
    return found;
}

[[nodiscard]] const ScriptApi& api()
{
    static const ScriptApi declared{oui::read_file(std::string{OLAGARRO_SOURCE_DIR} + "/oui/oui.d.ts").value_or("")};
    return declared;
}

TEST(CompletionTests, TellsWhatTheCursorIsWriting)
{
    const std::string text{"let file = oui.rea"};
    auto writing{writing_at(text, text.size())};
    EXPECT_EQ(writing.prefix, "rea");
    EXPECT_EQ(writing.qualifier, "oui");
    EXPECT_EQ(writing.start, text.size() - 3);
    writing = writing_at("oui.languages.reg", 17);
    EXPECT_EQ(writing.qualifier, "oui.languages");
    writing = writing_at("x = oui.", 8);
    EXPECT_EQ(writing.prefix, "");
    EXPECT_EQ(writing.qualifier, "oui");
    writing = writing_at("plain wor", 9);
    EXPECT_EQ(writing.prefix, "wor");
    EXPECT_EQ(writing.qualifier, "");
    const std::string spanish{"año ñandú"};
    writing = writing_at(spanish, spanish.size());
    EXPECT_EQ(writing.prefix, "ñandú") << "letters beyond ASCII are part of a word";
}

TEST(CompletionTests, ReadsWhatScriptsCanUseFromTheDeclarations)
{
    const auto* const oui{api().members("oui")};
    ASSERT_NE(oui, nullptr);
    const auto read{std::find_if(oui->begin(), oui->end(), [](const Suggestion& s) { return s.label == "readFile"; })};
    ASSERT_NE(read, oui->end());
    EXPECT_EQ(read->detail, "(path: string): string | null");
    EXPECT_NE(labels(*oui), std::vector<std::string>{});
    const auto top{labels(*api().members(""))};
    for(const auto* name : {"setTimeout", "console", "oui"})
    {
        EXPECT_NE(std::find(top.begin(), top.end(), name), top.end()) << name;
    }
    EXPECT_NE(api().members("oui.languages"), nullptr);
    EXPECT_EQ(api().members("oui.Label"), nullptr) << "a class's members are its instances'";
}

TEST(CompletionTests, SuggestsWhatIsUnderANameAndTheWordsOfTheDocuments)
{
    auto found{labels(suggestions_for(writing_at("oui.re", 6), {}, nullptr, &api()))};
    EXPECT_EQ(found, (std::vector<std::string>{"readFile"}));
    found = labels(suggestions_for(writing_at("oui.", 4), {}, nullptr, &api()));
    EXPECT_GT(found.size(), 10u) << "after the dot, all there is under it";
    const std::string document{"value Validate validation va x"};
    found = labels(suggestions_for(writing_at("va", 2), {document}, nullptr, nullptr));
    EXPECT_EQ(found, (std::vector<std::string>{"validation", "value", "Validate"}))
        << "those that start exactly as written first, the word itself and short ones left out";
    EXPECT_TRUE(suggestions_for(writing_at("", 0), {document}, nullptr, nullptr).empty()) << "no word begun, no words";
}

// What a name holds when a document makes it as one of the API's classes, with what the class derives.
TEST(CompletionTests, SuggestsWhatAnInstanceOfTheApisClassesHas)
{
    const std::string document{"const label = new oui.Label(\"hi\");\nlabel."};
    const auto found{labels(suggestions_for(writing_at(document, document.size()), {document}, nullptr, &api()))};
    for(const auto* name : {"text", "focus", "id"})
    {
        EXPECT_NE(std::find(found.begin(), found.end(), name), found.end()) << name;
    }
    EXPECT_EQ(std::find(found.begin(), found.end(), "append"), found.end()) << "a TextEdit's, not a Label's";
    const std::string other{"const notes = new oui.TextEdit({});\nnotes.ap"};
    EXPECT_EQ(labels(suggestions_for(writing_at(other, other.size()), {other}, nullptr, &api())),
              (std::vector<std::string>{"append"}));
}

TEST(CompletionTests, ALanguageSuggestsItsWordsAndPiecesOfText)
{
    const std::vector<Suggestion> language{{"function", "keyword", ""},
                                           {"for", "loop", "for(let i = 0; i < $0; ++ i) {\n}"}};
    const auto found{suggestions_for(writing_at("fo", 2), {"format"}, &language, nullptr)};
    ASSERT_EQ(labels(found), (std::vector<std::string>{"for", "format"}));
    EXPECT_EQ(found[0].detail, "loop");
    EXPECT_NE(found[0].text.find("$0"), std::string::npos);
}

TEST(CompletionTests, FindsTheCallTheCursorIsInAndWhichArgument)
{
    auto at_end{[](std::string_view text) { return call_at(text, text.size()); }};
    auto call{at_end("const text = oui.readFile(")};
    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->name, "readFile");
    EXPECT_EQ(call->qualifier, "oui");
    EXPECT_EQ(call->argument, 0u);
    EXPECT_EQ(call->open, 25u);
    EXPECT_FALSE(call->made);
    call = at_end("f(a, g(b, c), {x: 1, y: 2}, ");
    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->name, "f");
    EXPECT_EQ(call->argument, 3u) << "the commas inside other brackets are theirs";
    call = at_end("oui.column([a, b");
    ASSERT_TRUE(call.has_value()) << "in a list inside the call";
    EXPECT_EQ(call->name, "column");
    call = at_end("f(\"(,\", '(', `(`, // (\n /* ( */ ");
    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->name, "f");
    EXPECT_EQ(call->argument, 3u) << "strings and comments hold no brackets nor commas";
    call = at_end("label = new oui.Label(");
    ASSERT_TRUE(call.has_value());
    EXPECT_TRUE(call->made);
    EXPECT_FALSE(at_end("f(a) ").has_value()) << "the call is closed";
    EXPECT_FALSE(at_end("x = (a, ").has_value()) << "brackets with no name before them are no call";
    EXPECT_FALSE(at_end("renew(").value_or(Call{}).made);
}

TEST(CompletionTests, KnowsHowWhatTheApiDeclaresIsCalled)
{
    const std::vector<std::string_view> none;
    auto signature{api().signature(*call_at("oui.readFile(", 13), none)};
    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(signature->text, "readFile(path: string): string | null");
    ASSERT_EQ(signature->parameters.size(), 1u);
    EXPECT_EQ(signature->text.substr(signature->parameters[0].first,
                                     signature->parameters[0].second - signature->parameters[0].first),
              "path: string");
    EXPECT_EQ(signature->doc, std::vector<std::string>{"The file's text, or null when it cannot be read."})
        << "without the line of which scripts have it";

    signature = api().signature(*call_at("oui.choose(", 11), none);
    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(signature->parameters.size(), 2u) << "the commas of the type of options are not between parameters";

    signature = api().signature(*call_at("console.log(a, b, ", 18), none);
    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(signature->parameter_of(2), 0u) << "any number of values go to the one parameter";

    signature = api().signature(*call_at("setTimeout(() => {}, ", 21), none);
    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(signature->parameter_of(1), 1u);
    EXPECT_EQ(signature->parameters.size(), 3u) << "the arrow of a function's type closes no bracket";

    signature = api().signature(*call_at("new oui.Label(", 14), none);
    ASSERT_TRUE(signature.has_value());
    EXPECT_EQ(signature->text.rfind("Label(text?: string", 0), 0u);
    EXPECT_FALSE(signature->doc.empty()) << "what the class is";

    const std::string script{"const output = new oui.TextEdit();\noutput.append("};
    signature = api().signature(*call_at(script, script.size()), {script});
    ASSERT_TRUE(signature.has_value()) << "what an instance a document makes has";
    EXPECT_EQ(signature->text.rfind("append(", 0), 0u);

    EXPECT_FALSE(api().signature(*call_at("if(", 3), none).has_value());
}

} // namespace

namespace
{

// An API of images, as a paint program's plugins have: a namespace with a function, a class, and a class whose
// methods are callbacks a script writes.
const char* const image_api{R"ts(declare namespace paint {
    function clouds(width: number, height: number): Image;
    function effect(definition: Definition): void;
    class Image {
        width: number;
        gaussianBlur(radius: number): Image;
        over(below: Image, opacity?: number): Image;
    }
    class Definition {
        name: string;
        render(image: Image, values: object): Image;
    }
}
)ts"};

} // namespace

TEST(CompletionTests, WhatAFunctionOrMethodReturnsIsKnownByWhatItIsDeclaredToReturn)
{
    const ScriptApi api{image_api};
    const std::string_view document{"const sky = paint.clouds(64, 64);\nconst soft = sky.gaussianBlur(2);\n"};
    EXPECT_EQ(api.type_of("sky", {document}), "paint.Image");
    EXPECT_EQ(api.type_of("soft", {document}), "paint.Image") << "a method of what a function returned";
    EXPECT_EQ(api.type_of("nothing", {document}), "");
    const std::string text{std::string{document} + "soft."};
    const auto found{labels(suggestions_for(writing_at(text, text.size()), {text}, nullptr, &api))};
    EXPECT_NE(std::find(found.begin(), found.end(), "gaussianBlur"), found.end());
    EXPECT_NE(std::find(found.begin(), found.end(), "width"), found.end());
}

TEST(CompletionTests, AParameterOfACallbackHasTheTypeItsMethodDeclares)
{
    const ScriptApi api{image_api};
    const std::string_view method{"paint.effect({\n    render(image, values) {\n        return image;\n    },\n});\n"};
    EXPECT_EQ(api.type_of("image", {method}), "paint.Image");
    EXPECT_EQ(api.type_of("values", {method}), "") << "an object is no class of the API";
    const std::string_view arrow{"paint.effect({render: (picture) => picture});\n"};
    EXPECT_EQ(api.type_of("picture", {arrow}), "paint.Image");
    const std::string_view called{"image.render(image, values);\n"};
    EXPECT_EQ(api.type_of("image", {called}), "") << "a call is not a function written";
    const std::string text{std::string{method} + "image.over("};
    const auto call{call_at(text, text.size())};
    ASSERT_TRUE(call);
    const auto signature{api.signature(*call, {text})};
    ASSERT_TRUE(signature);
    EXPECT_EQ(signature->text, "over(below: Image, opacity?: number): Image");
}

TEST(CompletionTests, WhatIsDeclaredCanBeLookedUpByItsWholeName)
{
    const ScriptApi api{"declare namespace paint {\n    /**\n     * Clouds.\n     */\n"
                        "    function clouds(width: number): Image;\n    class Image {\n        /**\n"
                        "         * Its width.\n         */\n        width: number;\n    }\n}\n"};
    const auto* const clouds{api.declared("paint.clouds")};
    ASSERT_NE(clouds, nullptr);
    EXPECT_EQ(clouds->text, "clouds(width: number): Image");
    EXPECT_EQ(clouds->doc, std::vector<std::string>{"Clouds."});
    const auto* const width{api.declared("paint.Image#width")};
    ASSERT_NE(width, nullptr) << "a field, too";
    EXPECT_EQ(width->text, "width: number");
    EXPECT_EQ(width->doc, std::vector<std::string>{"Its width."});
    EXPECT_EQ(api.declared("paint.nothing"), nullptr);
}
