#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "FakeData.h"
#include "FakeEeprom.h"
#include "mado/EepromTable.h"

namespace {

struct Record {
  int32_t value;
  char name[9];
  bool on;
};

using Table = mado::EepromTable<Record, 3U, FakeEeprom>;

Record make_record(FakeData& data) {
  Record record = {};
  record.value = static_cast<int32_t>(data.number(1U, 100000U));
  const std::string name = data.word(8U);
  static_cast<void>(memcpy(&record.name[0], name.c_str(), name.size()));
  record.on = (data.number(0U, 1U) == 1U);
  return record;
}

bool same(const Record& left, const Record& right) {
  return (left.value == right.value) && (strcmp(&left.name[0], &right.name[0]) == 0) &&
         (left.on == right.on);
}

TEST(EepromTable, FormatsAnUnformattedStoreAndReportsItFresh) {
  // Arrange
  FakeEeprom store;
  Table table(&store, 16U);

  // Act
  const bool existing = table.open();

  // Assert
  EXPECT_FALSE(existing);
  EXPECT_EQ(table.count(), 0U);
  EXPECT_EQ(store.read(16), mado::kEepromTableMagic);
}

TEST(EepromTable, KeepsRecordsAcrossARestart) {
  // Arrange
  FakeData data(20261105U);
  FakeEeprom store;
  const Record record = make_record(data);
  Table first(&store, 16U);
  static_cast<void>(first.open());
  const uint8_t slot = first.insert(record);
  Table second(&store, 16U);

  // Act
  const bool existing = second.open();

  // Assert
  Record loaded = {};
  EXPECT_TRUE(existing);
  EXPECT_EQ(slot, 1U);
  EXPECT_TRUE(second.load(slot, loaded));
  EXPECT_TRUE(same(loaded, record));
}

TEST(EepromTable, RefusesAnInsertWhenEverySlotIsTaken) {
  // Arrange
  FakeData data(20261106U);
  FakeEeprom store;
  Table table(&store, 0U);
  static_cast<void>(table.open());
  for (uint8_t i = 0U; i < Table::capacity(); ++i) {
    static_cast<void>(table.insert(make_record(data)));
  }

  // Act
  const uint8_t slot = table.insert(make_record(data));

  // Assert
  EXPECT_EQ(slot, 0U);
  EXPECT_EQ(table.count(), 3U);
}

TEST(EepromTable, ReusesAnErasedSlot) {
  // Arrange
  FakeData data(20261107U);
  FakeEeprom store;
  Table table(&store, 0U);
  static_cast<void>(table.open());
  static_cast<void>(table.insert(make_record(data)));
  static_cast<void>(table.insert(make_record(data)));
  const bool erased = table.erase(1U);

  // Act
  const uint8_t slot = table.insert(make_record(data));

  // Assert
  EXPECT_TRUE(erased);
  EXPECT_EQ(slot, 1U);
  EXPECT_EQ(table.count(), 2U);
}

TEST(EepromTable, SavesOnlyTheBytesThatChanged) {
  // Arrange
  FakeData data(20261108U);
  FakeEeprom store;
  Table table(&store, 0U);
  static_cast<void>(table.open());
  Record record = make_record(data);
  const uint8_t slot = table.insert(record);
  const size_t before = store.writes();
  record.on = !record.on;

  // Act
  const bool saved = table.save(slot, record);

  // Assert
  const size_t after = store.writes();
  EXPECT_TRUE(saved);
  EXPECT_EQ(after, before + 1U);
}

TEST(EepromTable, RefusesSlotsThatAreEmptyOrOutOfRange) {
  // Arrange
  FakeData data(20261109U);
  FakeEeprom store;
  Table table(&store, 0U);
  static_cast<void>(table.open());
  Record record = make_record(data);

  // Act
  const bool loaded = table.load(2U, record);

  // Assert
  EXPECT_FALSE(loaded);
  EXPECT_FALSE(table.load(0U, record));
  EXPECT_FALSE(table.load(4U, record));
  EXPECT_FALSE(table.save(2U, record));
  EXPECT_FALSE(table.erase(2U));
}

TEST(EepromTable, ReformatsWhenTheRecordShapeChanged) {
  // Arrange
  FakeData data(20261110U);
  FakeEeprom store;
  mado::EepromTable<Record, 2U, FakeEeprom> old_shape(&store, 0U);
  static_cast<void>(old_shape.open());
  static_cast<void>(old_shape.insert(make_record(data)));
  Table table(&store, 0U);

  // Act
  const bool existing = table.open();

  // Assert
  EXPECT_FALSE(existing);
  EXPECT_EQ(table.count(), 0U);
}

TEST(EepromTable, RefusesAStoreTooSmallForTheTable) {
  // Arrange
  FakeData data(20261111U);
  FakeEeprom store(Table::bytes() - 1U);
  Table table(&store, 0U);

  // Act
  const bool existing = table.open();

  // Assert
  EXPECT_FALSE(existing);
  EXPECT_EQ(table.insert(make_record(data)), 0U);
  EXPECT_EQ(table.count(), 0U);
}

}
