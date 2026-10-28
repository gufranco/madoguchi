#include <gtest/gtest.h>
#include <stdint.h>

#include <vector>

#include "../src/card/domain/file_system.policy.h"
#include "../src/card/domain/format_phase.entity.h"
#include "FakeData.h"

namespace {

class CardDomainTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

 private:
  FakeData data_{20261114U};
};

TEST_F(CardDomainTest, WalksEveryPhaseInOrder) {
  // Arrange
  std::vector<card::FormatPhase> seen;
  card::FormatPhase phase = card::FormatPhase::kFillZero;

  // Act
  for (uint8_t i = 0U; (i < card::kPhaseCount) && (phase != card::FormatPhase::kDone); ++i) {
    seen.push_back(phase);
    phase = card::phase_after(phase);
  }

  // Assert
  EXPECT_EQ(seen, (std::vector<card::FormatPhase>{
                      card::FormatPhase::kFillZero, card::FormatPhase::kCheckZero,
                      card::FormatPhase::kFillOne, card::FormatPhase::kCheckOne,
                      card::FormatPhase::kFormat, card::FormatPhase::kMount}));
}

TEST_F(CardDomainTest, WritesZerosFirstAndOnesSecond) {
  // Arrange
  const card::FormatPhase second = card::FormatPhase::kCheckOne;

  // Act
  const uint8_t pattern = card::pattern_of(second);

  // Assert
  EXPECT_EQ(pattern, 0xFFU);
  EXPECT_EQ(card::pattern_of(card::FormatPhase::kCheckZero), 0x00U);
}

TEST_F(CardDomainTest, CountsHalfOfTheFirstPassAsAnEighth) {
  // Arrange
  const uint32_t sectors = data().number(8U, 4096U) * 2U;

  // Act
  const uint8_t percent = card::percent_done(card::FormatPhase::kFillZero, sectors, sectors / 2U);

  // Assert
  EXPECT_EQ(percent, 12U);
}

TEST_F(CardDomainTest, FindsOneByteThatDiffers) {
  // Arrange
  uint8_t sector[card::kFormatSectorSize] = {};
  sector[data().number(0U, card::kFormatSectorSize - 1U)] = 0x01U;

  // Act
  const bool same = card::all_equal(0x00U, &sector[0], sizeof(sector));

  // Assert
  EXPECT_FALSE(same);
}

struct KindCase {
  uint8_t type;
  uint32_t sectors;
  card::CardKind kind;
};

class CardKindTest : public ::testing::TestWithParam<KindCase> {};

TEST_P(CardKindTest, NamesTheCardFamilyFromItsTypeAndSize) {
  // Arrange
  const KindCase sample = GetParam();

  // Act
  const card::CardKind kind = card::kind_for(card::CardGeometry{sample.type, sample.sectors});

  // Assert
  EXPECT_EQ(kind, sample.kind);
}

INSTANTIATE_TEST_SUITE_P(Families, CardKindTest,
                         ::testing::Values(KindCase{2U, 0x200000UL, card::CardKind::kSdsc},
                                           KindCase{3U, 0x3B72400UL, card::CardKind::kSdhc},
                                           KindCase{3U, 0x7720000UL, card::CardKind::kSdxc}));

TEST_F(CardDomainTest, ReportsCapacityInWholeMebibytes) {
  // Arrange
  const uint32_t sectors = (2048U * 3U) + 100U;

  // Act
  const int32_t mebibytes = card::capacity_mib(sectors);

  // Assert
  EXPECT_EQ(mebibytes, 3);
}

}
