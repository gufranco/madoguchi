#include <gtest/gtest.h>
#include <stdint.h>

#include "mado/GuardEntry.h"
#include "mado/LoginAttempt.h"
#include "mado/LoginGuard.h"
#include "mado/LoginGuardTable.h"

namespace {

using Table = mado::LoginGuardTable<2U>;

constexpr uint32_t kFirst = 0x0A000001UL;
constexpr uint32_t kSecond = 0x0A000002UL;
constexpr uint32_t kThird = 0x0A000003UL;
constexpr uint32_t kNow = 100U;

void fail(Table& table, const mado::LoginAttempt& attempt, const uint8_t times) {
  for (uint8_t i = 0U; i < times; ++i) {
    table.record_failure(attempt);
  }
}

TEST(LoginGuardTable, LocksOnlyTheAddressThatFailed) {
  // Arrange
  Table table;
  fail(table, {kFirst, kNow}, mado::LoginGuard::kMaxFailures);

  // Act
  const bool second = table.locked({kSecond, kNow});

  // Assert
  EXPECT_EQ(table.locked({kFirst, kNow}), true);
  EXPECT_EQ(second, false);
  EXPECT_EQ(table.retry_after({kFirst, kNow}), mado::LoginGuard::kLockSeconds);
}

TEST(LoginGuardTable, LocksANewAddressWhileEverySlotIsLocked) {
  // Arrange
  Table table;
  fail(table, {kFirst, kNow}, mado::LoginGuard::kMaxFailures);
  fail(table, {kSecond, kNow}, mado::LoginGuard::kMaxFailures);

  // Act
  const bool third = table.locked({kThird, kNow});

  // Assert
  EXPECT_EQ(third, true);
  EXPECT_EQ(table.retry_after({kThird, kNow}), mado::LoginGuard::kLockSeconds);
}

TEST(LoginGuardTable, ReusesTheSlotOfAnUnlockedAddressWithFewerFailures) {
  // Arrange
  Table table;
  fail(table, {kFirst, kNow}, mado::LoginGuard::kMaxFailures);
  fail(table, {kSecond, kNow}, 1U);

  // Act
  table.record_failure({kThird, kNow});

  // Assert
  mado::GuardEntry entries[2U] = {};
  table.snapshot(entries);
  EXPECT_EQ(entries[0].address, kFirst);
  EXPECT_EQ(entries[0].failures, mado::LoginGuard::kMaxFailures);
  EXPECT_EQ(entries[1].address, kThird);
  EXPECT_EQ(entries[1].failures, 1U);
}

TEST(LoginGuardTable, ClearsAnAddressOnSuccess) {
  // Arrange
  Table table;
  fail(table, {kFirst, kNow}, 3U);

  // Act
  table.record_success(kFirst);

  // Assert
  mado::GuardEntry entries[2U] = {};
  table.snapshot(entries);
  EXPECT_EQ(entries[0].failures, 0U);
}

TEST(LoginGuardTable, RestoresALockFromAStoredTable) {
  // Arrange
  const mado::GuardEntry stored[2U] = {{kSecond, mado::LoginGuard::kMaxFailures}, {0U, 0U}};
  Table table;

  // Act
  table.restore(stored);

  // Assert
  EXPECT_EQ(table.locked({kSecond, 0U}), true);
  EXPECT_EQ(table.locked({kFirst, 0U}), false);
}

}
