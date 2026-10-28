#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/items/domain/item.entity.h"
#include "FakeData.h"

namespace {

items::ItemDraft draft_named(const std::string& name, const bool switched_on) {
  items::ItemDraft draft = {};
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
  draft.on = switched_on;
  return draft;
}

class ItemEntityTest : public ::testing::Test {
 protected:
  FakeData data_{20261028U};
};

TEST_F(ItemEntityTest, AcceptsANameOfOneToSixteenCharacters) {
  // Arrange
  const items::ItemDraft draft =
      draft_named(data_.word(data_.number(1U, items::kNameSize - 1U)), true);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kValid);
}

TEST_F(ItemEntityTest, RefusesAnEmptyName) {
  // Arrange
  const items::ItemDraft draft = draft_named("", false);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kEmptyName);
}

TEST_F(ItemEntityTest, RefusesANameWithNoRoomForItsTerminator) {
  // Arrange
  const items::ItemDraft draft = draft_named(data_.word(items::kNameSize), true);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kUnterminatedName);
}

struct IdCase {
  uint8_t id;
  bool valid;
};

class ItemIdTest : public ::testing::TestWithParam<IdCase> {};

TEST_P(ItemIdTest, TellsAnItemIdFromAnyOtherNumber) {
  // Arrange
  const IdCase sample = GetParam();

  // Act
  const bool valid = items::is_item_id(sample.id);

  // Assert
  EXPECT_EQ(valid, sample.valid);
}

INSTANTIATE_TEST_SUITE_P(
    Bounds, ItemIdTest,
    ::testing::Values(IdCase{0U, false}, IdCase{1U, true}, IdCase{items::kCapacity, true},
                      IdCase{static_cast<uint8_t>(items::kCapacity + 1U), false}));

TEST_F(ItemEntityTest, BuildsAnItemFromADraftAndAnId) {
  // Arrange
  const std::string name = data_.word(6U);
  const items::ItemDraft draft = draft_named(name, true);

  // Act
  const items::Item item = items::item_from(draft, 3U);

  // Assert
  EXPECT_EQ(item.id, 3U);
  EXPECT_EQ(std::string(&item.name[0]), name);
  EXPECT_TRUE(item.on);
}

TEST_F(ItemEntityTest, GivesNoIdToAnItemBuiltWithAnInvalidOne) {
  // Arrange
  const items::ItemDraft draft = draft_named(data_.word(4U), false);

  // Act
  const items::Item item = items::item_from(draft, 0U);

  // Assert
  EXPECT_EQ(item.id, 0U);
}

}
