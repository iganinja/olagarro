#include <bytestream/bytestream.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <utility>

using namespace olagarro;

TEST(ByteStreamTests, SupportsExplicitReadAndWrite)
{
    ByteStream stream;
    stream.write(42).write(7);
    EXPECT_EQ(stream.read<int>(), 42);
    auto value{0};
    stream.read(value);
    EXPECT_EQ(value, 7);
}

TEST(ByteStreamTests, RejectsTruncatedReads)
{
    ByteStream stream;
    stream.write(1);
    stream.read<int>();
    auto value{0};
    EXPECT_THROW(stream.read(value), std::out_of_range);
}

// A file that could not be read whole left the stream with part of it.
TEST(ByteStreamTests, AFailedLoadLeavesTheStreamAsItWas)
{
    ByteStream stream;
    stream.write(42);
    EXPECT_FALSE(ByteStream::load_from_file(stream, "/no/such/folder/olagarro_bytestream.bin"));
    // A folder opens as a file on some systems and cannot be read as one.
    EXPECT_FALSE(ByteStream::load_from_file(stream, std::filesystem::temp_directory_path().string()));
    ASSERT_EQ(stream.size(), sizeof(int));
    EXPECT_EQ(stream.read<int>(), 42);
}

TEST(ByteStreamTests, AddingNoDataFromNowhereIsNothingButSomeIsAnError)
{
    ByteStream stream;
    stream.add_data(nullptr, 0);
    EXPECT_EQ(stream.size(), 0u);
    EXPECT_THROW(stream.add_data(nullptr, 4), std::invalid_argument);
}

// Moving copied the bytes, as there was no move constructor.
TEST(ByteStreamTests, MovingTakesTheBytesAlong)
{
    ByteStream stream;
    stream.write(1).write(2);
    const auto* const bytes{stream.data()};
    ByteStream moved{std::move(stream)};
    EXPECT_EQ(moved.data(), bytes);
    EXPECT_EQ(moved.read<int>(), 1);
    ByteStream assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.data(), bytes);
    EXPECT_EQ(assigned.read_index(), 0u) << "read from the start, as a copy is";
}
