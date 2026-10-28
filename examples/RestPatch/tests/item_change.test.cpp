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

items::ItemChange rename_to(const std::string& name) {
  items::ItemChange change = {};
  change.renames = true;
  static_cast<void>(memcpy(&change.name[0], name.data(), name.size()));
  return change;
}

items::ItemChange switch_to(const bool switched_on) {
  items::ItemChange change = {};
  change.switches = true;
  change.on = switched_on;
  return change;
}

class ItemChangeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    items::ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], first_.data(), first_.size()));
    draft.on = true;
    static_cast<void>(Catalog::add(repository_, draft));
  }

  FakeData& data() {
    return data_;
  }

  const std::string& first() const {
    return first_;
  }

  items::RamItemRepository& repository() {
    return repository_;
  }

  std::string stored_name(const uint8_t item_id) {
    const items::ItemLookup lookup = Catalog::find(repository_, item_id);
    return std::string(&lookup.item.name[0]);
  }

 private:
  FakeData data_{20261101U};
  const std::string first_ = data_.word(5U);
  items::RamItemRepository repository_;
};

TEST_F(ItemChangeTest, CallsAChangeWithNoFieldNothing) {
  // Arrange
  const items::ItemChange change = {};

  // Act
  const items::ChangeStatus status = items::change_status(change);

  // Assert
  EXPECT_EQ(status, items::ChangeStatus::kNothing);
}

TEST_F(ItemChangeTest, RefusesARenameToAnEmptyName) {
  // Arrange
  const items::ItemChange change = rename_to("");

  // Act
  const items::ChangeStatus status = items::change_status(change);

  // Assert
  EXPECT_EQ(status, items::ChangeStatus::kInvalidName);
}

TEST_F(ItemChangeTest, SwitchesWithoutTouchingTheName) {
  // Arrange
  const items::Item current = Catalog::find(repository(), 1U).item;

  // Act
  const items::Item next = items::changed(current, switch_to(false));

  // Assert
  EXPECT_FALSE(next.on);
  EXPECT_EQ(std::string(&next.name[0]), first());
}

TEST_F(ItemChangeTest, StoresARename) {
  // Arrange
  const std::string name = data().word(7U);

  // Act
  const items::ChangeResult result = Catalog::change(repository(), 1U, rename_to(name));

  // Assert
  EXPECT_EQ(result.status, items::ChangeOutcome::kChanged);
  EXPECT_EQ(stored_name(1U), name);
  EXPECT_TRUE(result.item.on);
}

TEST_F(ItemChangeTest, ReportsAnIdThatHoldsNoItem) {
  // Arrange
  const items::ItemChange change = switch_to(true);

  // Act
  const items::ChangeResult result = Catalog::change(repository(), 3U, change);

  // Assert
  EXPECT_EQ(result.status, items::ChangeOutcome::kMissing);
}

}
