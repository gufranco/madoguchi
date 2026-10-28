// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_SDFAT_CARD_DRIVER_H
#define REST_FORMAT_CARD_SDFAT_CARD_DRIVER_H

#include <SdFat.h>
#include <mado/check.h>
#include <stdint.h>

#include "../domain/file_system.policy.h"

namespace card {

constexpr uint8_t kSdChipSelect = 4U;
constexpr uint32_t kSdClockHz = 25000000UL;

struct SdFatCardDriver {
  using Device = SdCard;

  static Device* open() {
    static SdCardFactory factory;
    Device* const card = factory.newCard(SdSpiConfig(kSdChipSelect, SHARED_SPI, kSdClockHz));
    return mado::check(card != nullptr) ? card : nullptr;
  }

  static bool format(Device& device, const CardFileSystem kind, uint8_t* const sector) {
    const bool valid = mado::check(sector != nullptr) && mado::check(kind != CardFileSystem::kNone);
    if (!valid) {
      return false;
    }
    if (kind == CardFileSystem::kExFat) {
      ExFatFormatter formatter;
      return formatter.format(&device, sector, nullptr);
    }
    FatFormatter formatter;
    return formatter.format(&device, sector, nullptr);
  }

  static CardFileSystem mounted(Device& device) {
    static FsVolume volume;
    const bool opened = mado::check(volume.begin(&device));
    const uint8_t type = opened ? volume.fatType() : 0U;
    switch (type) {
      case FAT_TYPE_FAT16: return CardFileSystem::kFat16;
      case FAT_TYPE_FAT32: return CardFileSystem::kFat32;
      case FAT_TYPE_EXFAT: return CardFileSystem::kExFat;
      default: return CardFileSystem::kNone;
    }
  }
};

}

#endif
