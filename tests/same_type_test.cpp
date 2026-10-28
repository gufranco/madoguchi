#include <gtest/gtest.h>
#include <stdint.h>

#include "mado/same_type.h"

namespace {

TEST(SameType, HoldsForOneTypeNamedTwice) {
  // Arrange
  using Left = uint8_t;

  // Act
  const bool same = mado::same_type<Left, uint8_t>::value;

  // Assert
  EXPECT_TRUE(same);
}

TEST(SameType, FailsForTypesOfTheSameWidth) {
  // Arrange
  using Left = uint8_t;

  // Act
  const bool same = mado::same_type<Left, int8_t>::value;

  // Assert
  EXPECT_FALSE(same);
}

TEST(SameType, TellsAReferenceFromItsValueType) {
  // Arrange
  using Left = const bool&;

  // Act
  const bool same = mado::same_type<Left, bool>::value;

  // Assert
  EXPECT_FALSE(same);
}

}
