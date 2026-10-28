#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeData.h"

namespace {

using Catalog = items::ItemCatalog<items::RamItemRepository>;

items::ItemDraft draft_named(const std::string& name, const bool switched_on) {
  items::ItemDraft draft = {};
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
  draft.on = switched_on;
  return draft;
}

class ItemReplacementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static_cast<void>(Catalog::add(repository_, draft_named(data_.word(4U), true)));
  }

  FakeData& data() {
    return data_;
  }

  items::RamItemRepository& repository() {
    return repository_;
  }

  std::string stored_name(const uint8_t item_id) {
    const items::ItemLookup lookup = Catalog::find(repository_, item_id);
    return std::string(&lookup.item.name[0]);
  }

 private:
  FakeData data_{20261031U};
  items::RamItemRepository repository_;
};

TEST_F(ItemReplacementTest, KeepsTheIdOfTheItemItReplaces) {
  // Arrange
  const items::Item current = Catalog::find(repository(), 1U).item;
  const std::string name = data().word(6U);

  // Act
  const items::Item next = items::replaced(current, draft_named(name, false));

  // Assert
  EXPECT_EQ(next.id, 1U);
  EXPECT_EQ(std::string(&next.name[0]), name);
  EXPECT_FALSE(next.on);
}

TEST_F(ItemReplacementTest, StoresTheReplacement) {
  // Arrange
  const std::string name = data().word(5U);

  // Act
  const items::ReplaceResult result = Catalog::replace(repository(), 1U, draft_named(name, false));

  // Assert
  EXPECT_EQ(result.status, items::ReplaceStatus::kReplaced);
  EXPECT_EQ(stored_name(1U), name);
}

TEST_F(ItemReplacementTest, RefusesAnEmptyName) {
  // Arrange
  const items::ItemDraft draft = draft_named("", true);

  // Act
  const items::ReplaceResult result = Catalog::replace(repository(), 1U, draft);

  // Assert
  EXPECT_EQ(result.status, items::ReplaceStatus::kInvalid);
}

TEST_F(ItemReplacementTest, ReportsAnIdThatHoldsNoItem) {
  // Arrange
  const items::ItemDraft draft = draft_named(data().word(3U), true);

  // Act
  const items::ReplaceResult result = Catalog::replace(repository(), 2U, draft);

  // Assert
  EXPECT_EQ(result.status, items::ReplaceStatus::kMissing);
}

TEST_F(ItemReplacementTest, RaisesTheRevisionOnEveryReplacement) {
  // Arrange
  const uint32_t before = Catalog::find(repository(), 1U).item.revision;

  // Act
  const items::ReplaceResult result =
      Catalog::replace(repository(), 1U, draft_named(data().word(5U), false));

  // Assert
  EXPECT_EQ(result.status, items::ReplaceStatus::kReplaced);
  EXPECT_EQ(result.item.revision, before + 1U);
  EXPECT_EQ(Catalog::find(repository(), 1U).item.revision, before + 1U);
}

TEST(ItemRevision, WrapsToOneInsteadOfZero) {
  // Arrange
  const uint32_t last = UINT32_MAX;

  // Act
  const uint32_t next = items::next_revision(last);

  // Assert
  EXPECT_EQ(next, 1U);
}

TEST(ItemRevision, StartsFromTheSeedTheRepositoryWasGiven) {
  // Arrange
  items::RamItemRepository repository;
  repository.begin(0x1000U);

  // Act
  const items::AddResult added = Catalog::add(repository, draft_named("a", true));

  // Assert
  EXPECT_EQ(added.item.revision, 0x1000U);
}

}
