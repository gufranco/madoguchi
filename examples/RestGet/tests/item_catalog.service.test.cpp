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

class ItemCatalogTest : public ::testing::Test {
 protected:
  FakeData data_{20261029U};
  items::RamItemRepository repository_;
};

TEST_F(ItemCatalogTest, AddsAnItemUnderTheFirstFreeId) {
  // Arrange
  const items::ItemDraft draft = draft_named(data_.word(5U), true);

  // Act
  const items::AddResult result = Catalog::add(repository_, draft);

  // Assert
  EXPECT_EQ(result.status, items::AddStatus::kAdded);
  EXPECT_EQ(result.item.id, 1U);
  EXPECT_TRUE(result.item.on);
}

TEST_F(ItemCatalogTest, RefusesADraftWithAnEmptyName) {
  // Arrange
  const items::ItemDraft draft = draft_named("", true);

  // Act
  const items::AddResult result = Catalog::add(repository_, draft);

  // Assert
  EXPECT_EQ(result.status, items::AddStatus::kInvalid);
  EXPECT_FALSE(Catalog::find(repository_, 1U).found);
}

TEST_F(ItemCatalogTest, ReportsAFullCatalog) {
  // Arrange
  for (uint8_t i = 0U; i < items::kCapacity; ++i) {
    static_cast<void>(Catalog::add(repository_, draft_named(data_.word(3U), false)));
  }

  // Act
  const items::AddResult result = Catalog::add(repository_, draft_named(data_.word(3U), false));

  // Assert
  EXPECT_EQ(result.status, items::AddStatus::kFull);
}

TEST_F(ItemCatalogTest, FindsAnAddedItem) {
  // Arrange
  const std::string name = data_.word(7U);
  static_cast<void>(Catalog::add(repository_, draft_named(name, true)));

  // Act
  const items::ItemLookup lookup = Catalog::find(repository_, 1U);

  // Assert
  EXPECT_TRUE(lookup.found);
  EXPECT_EQ(std::string(&lookup.item.name[0]), name);
}

TEST_F(ItemCatalogTest, FindsNothingOutsideTheIdRange) {
  // Arrange
  static_cast<void>(Catalog::add(repository_, draft_named(data_.word(4U), true)));

  // Act
  const items::ItemLookup lookup = Catalog::find(repository_, 0U);

  // Assert
  EXPECT_FALSE(lookup.found);
}

TEST_F(ItemCatalogTest, FindsNothingUnderAFreeId) {
  // Arrange
  static_cast<void>(Catalog::add(repository_, draft_named(data_.word(4U), true)));

  // Act
  const items::ItemLookup lookup = Catalog::find(repository_, 2U);

  // Assert
  EXPECT_FALSE(lookup.found);
}

}
