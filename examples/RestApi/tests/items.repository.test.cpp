#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/items/adapters/card_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeData.h"
#include "FakeSd.h"

namespace {

constexpr uint32_t kSession = 41U;

using Repository = items::CardItemRepository<FakeSd, FILE_READ, FILE_WRITE>;
using Catalog = items::ItemCatalog<Repository>;

items::ItemDraft draft_named(const std::string& name, const int32_t value) {
  items::ItemDraft draft = {};
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
  draft.value = value;
  draft.on = true;
  return draft;
}

class ItemsRepositoryTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

  FakeSd& card() {
    return card_;
  }

  Repository& repository() {
    return repository_;
  }

 private:
  FakeData data_{20261112U};
  FakeSd card_;
  Repository repository_;
};

TEST_F(ItemsRepositoryTest, WritesAnAddedItemToItsOwnFile) {
  // Arrange
  static_cast<void>(repository().attach(&card(), kSession));
  const std::string name = data().word(5U);

  // Act
  const items::Result result = Catalog::add(repository(), draft_named(name, 9));

  // Assert
  EXPECT_TRUE(result.saved);
  EXPECT_EQ(card().files()["/ITEM1.TXT"], R"({"name":")" + name +
                                              R"(","value":9,"on":true})"
                                              "\n");
}

TEST_F(ItemsRepositoryTest, LoadsWhatAnEarlierBootSaved) {
  // Arrange
  card().files()["/ITEM2.TXT"] = "{\"name\":\"fan\",\"value\":-4,\"on\":false}\n";

  // Act
  const uint8_t loaded = repository().attach(&card(), kSession);

  // Assert
  EXPECT_EQ(loaded, 1U);
  EXPECT_EQ(Catalog::find(repository(), 2U).item.value, -4);
}

TEST_F(ItemsRepositoryTest, ForgetsTheFileOfARemovedItem) {
  // Arrange
  static_cast<void>(repository().attach(&card(), kSession));
  static_cast<void>(Catalog::add(repository(), draft_named(data().word(4U), 1)));

  // Act
  const items::Result result = Catalog::remove(repository(), 1U);

  // Assert
  EXPECT_EQ(result.outcome, items::Outcome::kDone);
  EXPECT_EQ(card().files().count("/ITEM1.TXT"), 0U);
}

TEST_F(ItemsRepositoryTest, ReportsASaveTheCardRefused) {
  // Arrange
  static_cast<void>(repository().attach(&card(), kSession));
  card().limit_writes(4U);

  // Act
  const items::Result result = Catalog::add(repository(), draft_named(data().word(4U), 1));

  // Assert
  EXPECT_EQ(result.outcome, items::Outcome::kDone);
  EXPECT_FALSE(result.saved);
}

TEST_F(ItemsRepositoryTest, KeepsItemsInMemoryWithoutACard) {
  // Arrange
  static_cast<void>(repository().attach(nullptr, kSession));

  // Act
  const items::Result result = Catalog::add(repository(), draft_named(data().word(4U), 1));

  // Assert
  EXPECT_TRUE(result.saved);
  EXPECT_TRUE(Catalog::find(repository(), 1U).found);
  EXPECT_TRUE(card().files().empty());
}

}
