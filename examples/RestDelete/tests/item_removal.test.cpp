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

class ItemRemovalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string name = data_.word(4U);
    items::ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
    static_cast<void>(Catalog::add(repository_, draft));
  }

  FakeData& data() {
    return data_;
  }

  items::RamItemRepository& repository() {
    return repository_;
  }

 private:
  FakeData data_{20261102U};
  items::RamItemRepository repository_;
};

TEST_F(ItemRemovalTest, RemovesAnItem) {
  // Arrange
  const uint8_t item_id = 1U;

  // Act
  const items::RemoveStatus status = Catalog::remove(repository(), item_id);

  // Assert
  EXPECT_EQ(status, items::RemoveStatus::kRemoved);
  EXPECT_FALSE(Catalog::find(repository(), item_id).found);
}

TEST_F(ItemRemovalTest, ReportsTheSecondRemovalAsMissing) {
  // Arrange
  static_cast<void>(Catalog::remove(repository(), 1U));

  // Act
  const items::RemoveStatus status = Catalog::remove(repository(), 1U);

  // Assert
  EXPECT_EQ(status, items::RemoveStatus::kMissing);
}

TEST_F(ItemRemovalTest, ReportsAnIdOutsideTheRangeAsMissing) {
  // Arrange
  const uint8_t item_id = static_cast<uint8_t>(items::kCapacity + 1U);

  // Act
  const items::RemoveStatus status = Catalog::remove(repository(), item_id);

  // Assert
  EXPECT_EQ(status, items::RemoveStatus::kMissing);
}

TEST_F(ItemRemovalTest, FreesTheIdForTheNextItem) {
  // Arrange
  static_cast<void>(Catalog::remove(repository(), 1U));
  items::ItemDraft draft = {};
  const std::string name = data().word(3U);
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));

  // Act
  const items::AddResult result = Catalog::add(repository(), draft);

  // Assert
  EXPECT_EQ(result.item.id, 1U);
}

}
