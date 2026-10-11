#include <oui/application.h>
#include <oui/backend.h>
#include <oui/cell_buffer.h>
#include <oui/label.h>
#include <oui/table.h>
#include <oui/raster_backend.h>
#include <oui/window.h>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using namespace olagarro;

namespace
{

class FixedBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {30, 10}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
};

} // namespace

class ListMetadataTest : public ::testing::Test
{
  protected:
    void SetUp() override { m_app = std::make_unique<oui::Application>(std::make_unique<FixedBackend>()); }

    void TearDown() override { m_app.reset(); }

    std::unique_ptr<oui::Application> m_app;
};

TEST_F(ListMetadataTest, SetAndGetMetadata)
{
    auto list{oui::table()};
    list->add(oui::label("A"));

    list->set_metadata(0, 0, "key", 42);

    auto* val = list->metadata(0, 0, "key");
    ASSERT_NE(val, nullptr);
    ASSERT_TRUE(std::holds_alternative<int>(*val));
    EXPECT_EQ(std::get<int>(*val), 42);
}

TEST_F(ListMetadataTest, GetOrDefaultReturnsDefaultWhenMissing)
{
    auto list{oui::table()};
    list->add(oui::label("A"));

    int val{list->metadata_or<int>(0, 0, "missing", 99)};
    EXPECT_EQ(val, 99);
}

TEST_F(ListMetadataTest, GetOrDefaultReturnsCorrectType)
{
    auto list{oui::table()};
    list->add(oui::label("A"));

    list->set_metadata(0, 0, "bool_val", true);
    list->set_metadata(0, 0, "double_val", 3.14);
    list->set_metadata(0, 0, "str_val", std::string("hello"));

    EXPECT_TRUE(list->metadata_or<bool>(0, 0, "bool_val", false));
    EXPECT_DOUBLE_EQ(list->metadata_or<double>(0, 0, "double_val", 0.0), 3.14);
    EXPECT_EQ(list->metadata_or<std::string>(0, 0, "str_val", "default"), "hello");
}

TEST_F(ListMetadataTest, ClearMetadataWorks)
{
    auto list{oui::table()};
    list->add(oui::label("A"));

    list->set_metadata(0, 0, "key", 42);
    list->clear_metadata(0, 0);

    auto* val = list->metadata(0, 0, "key");
    EXPECT_EQ(val, nullptr);
}

TEST_F(ListMetadataTest, ClearAllMetadataWorks)
{
    auto list{oui::table()};
    list->add(oui::label("A"));
    list->add(oui::label("B"));

    list->set_metadata(0, 0, "k1", 1);
    list->set_metadata(1, 0, "k2", 2);
    list->clear_all_metadata();

    EXPECT_EQ(list->metadata(0, 0, "k1"), nullptr);
    EXPECT_EQ(list->metadata(1, 0, "k2"), nullptr);
}
