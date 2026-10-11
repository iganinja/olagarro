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

class ListSortTest : public ::testing::Test
{
  protected:
    void SetUp() override { m_app = std::make_unique<oui::Application>(std::make_unique<FixedBackend>()); }

    void TearDown() override { m_app.reset(); }

    std::unique_ptr<oui::Application> m_app;
};

TEST_F(ListSortTest, DefaultAlphaSortOrdersLabels)
{
    auto list{oui::table()};
    list->add(oui::label("Cherry"));
    list->add(oui::label("Apple"));
    list->add(oui::label("Banana"));

    list->set_column_sort(0, oui::SortOrder::ascending);
    list->apply_sort(0);

    auto& first{list->child_at(0)};
    auto& second{list->child_at(1)};
    auto& third{list->child_at(2)};

    if(auto* la = dynamic_cast<const oui::Label*>(&first))
    {
        EXPECT_STREQ(la->text().data(), "Apple");
    }
    if(auto* lb = dynamic_cast<const oui::Label*>(&second))
    {
        EXPECT_STREQ(lb->text().data(), "Banana");
    }
    if(auto* lc = dynamic_cast<const oui::Label*>(&third))
    {
        EXPECT_STREQ(lc->text().data(), "Cherry");
    }
}

TEST_F(ListSortTest, CustomComparatorWorks)
{
    auto list{oui::table()};
    list->add(oui::label("A"));
    list->add(oui::label("BB"));
    list->add(oui::label("CCC"));

    list->set_column_sort(0, oui::SortOrder::ascending,
                          [](const oui::Widget* a, const oui::Widget* b) -> int
                          {
                              if(auto* la = dynamic_cast<const oui::Label*>(a))
                                  if(auto* lb = dynamic_cast<const oui::Label*>(b))
                                  {
                                      return static_cast<int>(la->text().size()) - static_cast<int>(lb->text().size());
                                  }
                              return 0;
                          });

    list->apply_sort(0);

    auto& first{list->child_at(0)};
    if(auto* la = dynamic_cast<const oui::Label*>(&first))
    {
        EXPECT_STREQ(la->text().data(), "A");
    }
    auto& third{list->child_at(2)};
    if(auto* lc = dynamic_cast<const oui::Label*>(&third))
    {
        EXPECT_STREQ(lc->text().data(), "CCC");
    }
}

TEST_F(ListSortTest, DescendingOrderReverses)
{
    auto list{oui::table()};
    list->add(oui::label("A"));
    list->add(oui::label("B"));
    list->add(oui::label("C"));

    list->set_column_sort(0, oui::SortOrder::descending);
    list->apply_sort(0);

    if(auto* la = dynamic_cast<const oui::Label*>(&list->child_at(0)))
    {
        EXPECT_STREQ(la->text().data(), "C");
    }
    if(auto* lc = dynamic_cast<const oui::Label*>(&list->child_at(2)))
    {
        EXPECT_STREQ(lc->text().data(), "A");
    }
}

TEST_F(ListSortTest, MetadataSortingUsesIntegerValues)
{
    auto list{oui::table()};
    list->add(oui::label("Cherry"));
    list->add(oui::label("Apple"));
    list->add(oui::label("Banana"));

    list->set_metadata(0, 0, "value", 10);
    list->set_metadata(1, 0, "value", 100);
    list->set_metadata(2, 0, "value", 50);

    list->set_column_sort(0, oui::SortOrder::ascending);

    list->apply_sort(0);

    const auto* first = dynamic_cast<const oui::Label*>(&list->child_at(0));
    ASSERT_NE(first, nullptr);
    EXPECT_STREQ(first->text().data(), "Apple");
    auto* val = list->metadata(0, 0, "value");
    ASSERT_NE(val, nullptr);
    ASSERT_TRUE(std::holds_alternative<int>(*val));
    EXPECT_EQ(std::get<int>(*val), 100);
}
