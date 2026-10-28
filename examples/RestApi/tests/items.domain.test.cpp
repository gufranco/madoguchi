#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/items/domain/item.entity.h"
#include "FakeData.h"

namespace {

items::ItemDraft draft_named(const std::string& name, const int32_t value) {
  items::ItemDraft draft = {};
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
  draft.value = value;
  return draft;
}

class ItemsDomainTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

 private:
  FakeData data_{20261111U};
};

TEST_F(ItemsDomainTest, AcceptsAValueAtEitherLimit) {
  // Arrange
  const items::ItemDraft draft = draft_named(data().word(5U), items::kMaxValue);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kValid);
  EXPECT_EQ(items::draft_status(draft_named(data().word(5U), items::kMinValue)),
            items::DraftStatus::kValid);
}

TEST_F(ItemsDomainTest, RefusesAValuePastTheLimit) {
  // Arrange
  const items::ItemDraft draft = draft_named(data().word(5U), items::kMaxValue + 1);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kValueOutOfRange);
}

TEST_F(ItemsDomainTest, RefusesAnEmptyName) {
  // Arrange
  const items::ItemDraft draft = draft_named("", 1);

  // Act
  const items::DraftStatus status = items::draft_status(draft);

  // Assert
  EXPECT_EQ(status, items::DraftStatus::kEmptyName);
}

TEST_F(ItemsDomainTest, KeepsEveryFieldAChangeLeavesOut) {
  // Arrange
  const std::string name = data().word(6U);
  const items::Item item = items::item_from(draft_named(name, 7), 2U);
  items::ItemChange change = {};
  change.switches = true;
  change.on = true;

  // Act
  const items::ItemDraft merged = items::draft_of(item, change);

  // Assert
  EXPECT_EQ(std::string(&merged.name[0]), name);
  EXPECT_EQ(merged.value, 7);
  EXPECT_TRUE(merged.on);
}

TEST_F(ItemsDomainTest, CallsAChangeWithNoFieldNothing) {
  // Arrange
  const items::Item item = items::item_from(draft_named(data().word(4U), 0), 1U);

  // Act
  const items::ChangeStatus status = items::change_status(item, items::ItemChange{});

  // Assert
  EXPECT_EQ(status, items::ChangeStatus::kNothing);
}

TEST_F(ItemsDomainTest, RefusesAChangeThatEmptiesTheName) {
  // Arrange
  const items::Item item = items::item_from(draft_named(data().word(4U), 0), 1U);
  items::ItemChange change = {};
  change.renames = true;

  // Act
  const items::ChangeStatus status = items::change_status(item, change);

  // Assert
  EXPECT_EQ(status, items::ChangeStatus::kInvalid);
}

TEST(ItemRevision, SpreadsTheFirstRevisionOfEachSession) {
  // Arrange
  const uint32_t session = 1U;

  // Act
  const uint32_t first = items::first_revision(session);

  // Assert
  EXPECT_EQ(first, 0x9E3779B1UL);
}

TEST(ItemRevision, WrapsToOneInsteadOfZero) {
  // Arrange
  const uint32_t last = UINT32_MAX;

  // Act
  const uint32_t next = items::next_revision(last);

  // Assert
  EXPECT_EQ(next, 1U);
}

}
