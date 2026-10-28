// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_FILE_SYSTEM_POLICY_H
#define REST_FORMAT_CARD_FILE_SYSTEM_POLICY_H

#include <mado/check.h>
#include <stdint.h>

namespace card {

enum class CardFileSystem : uint8_t { kNone, kFat16, kFat32, kExFat };

enum class CardKind : uint8_t { kSdsc, kSdhc, kSdxc };

constexpr uint32_t kFat16SectorLimit = 0x400000UL;
constexpr uint32_t kFat32SectorLimit = 0x4000000UL;
constexpr uint32_t kSectorsPerMib = 2048UL;
constexpr uint8_t kSdhcType = 3U;

static_assert(kFat16SectorLimit < kFat32SectorLimit, "FAT16 cards are smaller than FAT32 cards");

inline CardFileSystem file_system_for(const uint32_t sectors) {
  if (!mado::check(sectors > 0U)) {
    return CardFileSystem::kNone;
  }
  if (mado::check(sectors < kFat16SectorLimit)) {
    return CardFileSystem::kFat16;
  }
  return (sectors <= kFat32SectorLimit) ? CardFileSystem::kFat32 : CardFileSystem::kExFat;
}

struct CardGeometry {
  uint8_t type;
  uint32_t sectors;
};

inline CardKind kind_for(const CardGeometry& card) {
  const bool high_capacity =
      mado::check(card.type <= kSdhcType) && mado::check(card.type == kSdhcType);
  if (!high_capacity) {
    return CardKind::kSdsc;
  }
  return (card.sectors > kFat32SectorLimit) ? CardKind::kSdxc : CardKind::kSdhc;
}

inline int32_t capacity_mib(const uint32_t sectors) {
  const bool present = mado::check(sectors > 0U);
  return present ? static_cast<int32_t>(sectors / kSectorsPerMib) : 0;
}

}

#endif
