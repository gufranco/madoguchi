#include <gtest/gtest.h>

#include "mado/Precondition.h"
#include "mado/if_match.h"

namespace {

struct MatchCase {
  const char* header = nullptr;
  const char* current = nullptr;
  mado::Precondition expected = mado::Precondition::kAbsent;
};

class IfMatchTest : public ::testing::TestWithParam<MatchCase> {};

TEST_P(IfMatchTest, ComparesTagsStrongly) {
  // Arrange
  const MatchCase& sample = GetParam();

  // Act
  const mado::Precondition outcome = mado::if_match(sample.header, sample.current);

  // Assert
  EXPECT_EQ(outcome, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, IfMatchTest,
    ::testing::Values(MatchCase{nullptr, "\"a1\"", mado::Precondition::kAbsent},
                      MatchCase{nullptr, nullptr, mado::Precondition::kAbsent},
                      MatchCase{"\"a1\"", "\"a1\"", mado::Precondition::kHolds},
                      MatchCase{"\"zz\", \"a1\"", "\"a1\"", mado::Precondition::kHolds},
                      MatchCase{"W/\"a1\"", "\"a1\"", mado::Precondition::kFails},
                      MatchCase{"\"a2\"", "\"a1\"", mado::Precondition::kFails},
                      MatchCase{"*", "\"a1\"", mado::Precondition::kHolds},
                      MatchCase{"*", nullptr, mado::Precondition::kFails},
                      MatchCase{"\"a1\"", nullptr, mado::Precondition::kFails},
                      MatchCase{"\"a1\"", "W/\"a1\"", mado::Precondition::kFails},
                      MatchCase{"", "\"a1\"", mado::Precondition::kFails}));

}
