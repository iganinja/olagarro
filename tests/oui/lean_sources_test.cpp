#include <oui/file_system.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

using namespace olagarro;

namespace
{

// OUI and its programs keep clear of C++'s streams and std::regex: in a static build they bring the whole
// machinery of locales along, some 200 KB of a program of two megabytes. Files go through C's, text through OUI's
// own, and regular expressions through oui::Regex.
TEST(OuiLeanSourcesTests, NothingBringsStreamsOrStdRegex)
{
    constexpr std::array<std::string_view, 8> heavy{"<fstream>", "<sstream>", "<iostream>", "<istream>",
                                                    "<ostream>", "<iomanip>", "<locale>",   "<regex>"};
    const std::filesystem::path root{OLAGARRO_SOURCE_DIR};
    for(const auto* folder : {"oui", "examples"})
    {
        for(const auto& entry : std::filesystem::recursive_directory_iterator{root / folder})
        {
            const auto extension{entry.path().extension()};
            if(!entry.is_regular_file() || (extension != ".cpp" && extension != ".h"))
            {
                continue;
            }
            const auto text{oui::read_file(entry.path()).value_or("")};
            for(const auto header : heavy)
            {
                EXPECT_EQ(text.find("#include " + std::string{header}), std::string::npos)
                    << entry.path().lexically_relative(root).generic_string() << " includes " << header;
            }
        }
    }
}

} // namespace
