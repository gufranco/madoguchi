// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef FORMAT_JOB_H
#define FORMAT_JOB_H

#include <Arduino.h>
#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class CardFileSystem : uint8_t { kNone, kFat16, kFat32, kExFat };

enum class FormatPhase : uint8_t {
  kIdle,
  kFillZero,
  kCheckZero,
  kFillOne,
  kCheckOne,
  kFormat,
  kMount,
  kDone
};

constexpr uint32_t kFat16SectorLimit = 0x400000UL;
constexpr uint32_t kFat32SectorLimit = 0x4000000UL;
constexpr uint8_t kFormatBatch = 8U;
constexpr uint16_t kFormatSectorSize = 512U;
constexpr uint8_t kFormatPasses = 4U;
constexpr uint8_t kFormatPercent = 100U;

static_assert(kFat16SectorLimit < kFat32SectorLimit, "FAT16 cards are smaller than FAT32 cards");

inline CardFileSystem file_system_for(const uint32_t sectors) {
  if (sectors == 0U) {
    return CardFileSystem::kNone;
  }
  if (sectors < kFat16SectorLimit) {
    return CardFileSystem::kFat16;
  }
  return (sectors <= kFat32SectorLimit) ? CardFileSystem::kFat32 : CardFileSystem::kExFat;
}

inline uint8_t pass_of(const FormatPhase phase) {
  switch (phase) {
    case FormatPhase::kIdle: return 0U;
    case FormatPhase::kFillZero: return 0U;
    case FormatPhase::kCheckZero: return 1U;
    case FormatPhase::kFillOne: return 2U;
    case FormatPhase::kCheckOne: return 3U;
    default: return kFormatPasses;
  }
}

inline FormatPhase phase_after(const FormatPhase phase) {
  switch (phase) {
    case FormatPhase::kFillZero: return FormatPhase::kCheckZero;
    case FormatPhase::kCheckZero: return FormatPhase::kFillOne;
    case FormatPhase::kFillOne: return FormatPhase::kCheckOne;
    case FormatPhase::kCheckOne: return FormatPhase::kFormat;
    case FormatPhase::kFormat: return FormatPhase::kMount;
    default: return FormatPhase::kDone;
  }
}

inline uint8_t pattern_of(const FormatPhase phase) {
  const bool zero = (phase == FormatPhase::kFillZero) || (phase == FormatPhase::kCheckZero);
  return zero ? 0x00U : 0xFFU;
}

inline bool all_equal(const uint8_t* const data, const size_t length, const uint8_t value) {
  const bool valid = mado::check(data != nullptr) && mado::check(length > 0U);
  bool same = valid;
  for (size_t i = 0U; same && (i < length); ++i) {
    same = data[i] == value;
  }
  return same;
}

struct FormatProgress {
  FormatPhase phase;
  bool failed;
  uint8_t percent;
  uint32_t sectors;
  CardFileSystem file_system;
  const __FlashStringHelper* detail;
};

template <typename DriverT>
class FormatJob {
 public:
  using Device = typename DriverT::Device;

  bool start(Device& device) {
    const bool idle = mado::check(!busy());
    const uint32_t sectors = idle ? device.sectorCount() : 0U;
    const CardFileSystem kind = file_system_for(sectors);
    if (kind == CardFileSystem::kNone) {
      return false;
    }
    device_ = &device;
    sectors_ = sectors;
    file_system_ = kind;
    phase_ = FormatPhase::kFillZero;
    sector_ = 0U;
    failed_ = false;
    detail_ = nullptr;
    return true;
  }

  bool busy() const {
    return (!failed_) && (phase_ != FormatPhase::kIdle) && (phase_ != FormatPhase::kDone);
  }

  void step() {
    if (!busy()) {
      return;
    }
    const bool wired = mado::check(device_ != nullptr) && mado::check(sector_ < sectors_);
    if (!wired) {
      fail(F("the job lost its card"));
      return;
    }
    switch (phase_) {
      case FormatPhase::kFillZero: fill(); break;
      case FormatPhase::kFillOne: fill(); break;
      case FormatPhase::kCheckZero: compare(); break;
      case FormatPhase::kCheckOne: compare(); break;
      case FormatPhase::kFormat: format(); break;
      default: mount(); break;
    }
  }

  FormatProgress progress() const {
    const uint64_t total = static_cast<uint64_t>(sectors_) * kFormatPasses;
    const uint64_t done = (static_cast<uint64_t>(pass_of(phase_)) * sectors_) + sector_;
    const bool counted = (total > 0U) && (phase_ != FormatPhase::kIdle);
    const uint64_t share = counted ? ((done * kFormatPercent) / total) : 0U;
    const uint8_t percent = static_cast<uint8_t>((share > kFormatPercent) ? kFormatPercent : share);
    return FormatProgress{phase_, failed_, percent, sectors_, file_system_, detail_};
  }

 private:
  uint32_t batch() const {
    const uint32_t remaining = sectors_ - sector_;
    return (remaining < kFormatBatch) ? remaining : kFormatBatch;
  }

  void advance(const uint32_t count) {
    const bool bounded = mado::check(count <= (sectors_ - sector_));
    sector_ = bounded ? (sector_ + count) : sectors_;
    if (sector_ >= sectors_) {
      sector_ = 0U;
      phase_ = phase_after(phase_);
    }
  }

  void fail(const __FlashStringHelper* const detail) {
    failed_ = true;
    detail_ = detail;
  }

  void fill() {
    const uint32_t count = batch();
    static_cast<void>(memset(&buffer_[0], pattern_of(phase_), sizeof(buffer_)));
    const bool written = device_->writeSectors(sector_, &buffer_[0], count);
    if (!written) {
      fail(F("the card refused a write"));
      return;
    }
    advance(count);
  }

  void compare() {
    const bool same = mado::check(device_->sectorCount() == sectors_);
    if (!same) {
      fail(F("the card changed during the wipe"));
      return;
    }
    const uint32_t count = batch();
    const bool read = device_->readSectors(sector_, &buffer_[0], count);
    if (!read) {
      fail(F("the card refused a read"));
      return;
    }
    const size_t length = static_cast<size_t>(count) * kFormatSectorSize;
    if (!all_equal(&buffer_[0], length, pattern_of(phase_))) {
      fail(F("the card read back a byte it was not given"));
      return;
    }
    advance(count);
  }

  void format() {
    const bool known = mado::check(file_system_ != CardFileSystem::kNone);
    const bool formatted = known && DriverT::format(*device_, file_system_, &buffer_[0]);
    if (!formatted) {
      fail(F("the format did not complete"));
      return;
    }
    phase_ = FormatPhase::kMount;
  }

  void mount() {
    const bool mounted = DriverT::mounted(*device_) == file_system_;
    if (!mounted) {
      fail(F("the new volume did not mount"));
      return;
    }
    phase_ = FormatPhase::kDone;
    detail_ = done_detail(file_system_);
  }

  static const __FlashStringHelper* done_detail(const CardFileSystem kind) {
    const bool known = mado::check(kind != CardFileSystem::kNone);
    if (!known) {
      return F("");
    }
    if (kind == CardFileSystem::kFat16) {
      return F("the card holds a fresh FAT16 volume");
    }
    return (kind == CardFileSystem::kFat32) ? F("the card holds a fresh FAT32 volume")
                                            : F("the card holds a fresh exFAT volume");
  }

  Device* device_ = nullptr;
  uint32_t sectors_ = 0U;
  uint32_t sector_ = 0U;
  FormatPhase phase_ = FormatPhase::kIdle;
  CardFileSystem file_system_ = CardFileSystem::kNone;
  bool failed_ = false;
  const __FlashStringHelper* detail_ = nullptr;
  uint8_t buffer_[kFormatBatch * kFormatSectorSize] = {};
};

#endif
