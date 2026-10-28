// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_FORMAT_PHASE_ENTITY_H
#define REST_FORMAT_CARD_FORMAT_PHASE_ENTITY_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>

#include "file_system.policy.h"

namespace card {

enum class FormatPhase : uint8_t {
  kIdle,
  kFillZero,
  kCheckZero,
  kFillOne,
  kCheckOne,
  kFormat,
  kMount,
  kDone,
};

enum class FormatFailure : uint8_t {
  kNone,
  kLostCard,
  kRefusedWrite,
  kCardChanged,
  kRefusedRead,
  kWrongByte,
  kFormatFailed,
  kNotMounted,
};

struct FormatProgress {
  FormatPhase phase;
  FormatFailure failure;
  uint8_t percent;
  uint32_t sectors;
  CardFileSystem file_system;
};

constexpr uint8_t kPhaseCount = 8U;
constexpr uint8_t kFailureCount = 8U;
constexpr uint8_t kFormatBatch = 8U;
constexpr uint16_t kFormatSectorSize = 512U;
constexpr uint8_t kFormatPasses = 4U;
constexpr uint8_t kFormatPercent = 100U;
constexpr uint8_t kZeroPattern = 0x00U;
constexpr uint8_t kOnePattern = 0xFFU;

inline bool known_phase(const FormatPhase phase) {
  return mado::check(static_cast<uint8_t>(phase) < kPhaseCount);
}

inline uint8_t pass_of(const FormatPhase phase) {
  if (!mado::check(known_phase(phase))) {
    return kFormatPasses;
  }
  switch (phase) {
    case FormatPhase::kIdle:
    case FormatPhase::kFillZero: return 0U;
    case FormatPhase::kCheckZero: return 1U;
    case FormatPhase::kFillOne: return 2U;
    case FormatPhase::kCheckOne: return 3U;
    default: return kFormatPasses;
  }
}

inline FormatPhase phase_after(const FormatPhase phase) {
  if (!mado::check(known_phase(phase))) {
    return FormatPhase::kDone;
  }
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
  const bool wiping = known_phase(phase) && mado::check(pass_of(phase) < kFormatPasses);
  const bool zero = (phase == FormatPhase::kFillZero) || (phase == FormatPhase::kCheckZero);
  return (wiping && zero) ? kZeroPattern : kOnePattern;
}

inline bool all_equal(const uint8_t value, const uint8_t* const data, const size_t length) {
  const bool valid = mado::check(data != nullptr) && mado::check(length > 0U);
  bool same = valid;
  for (size_t i = 0U; same && (i < length); ++i) {
    same = data[i] == value;
  }
  return same;
}

inline uint8_t percent_done(const FormatPhase phase, const uint32_t sectors,
                            const uint32_t sector) {
  const uint64_t total = static_cast<uint64_t>(sectors) * kFormatPasses;
  const uint64_t done = (static_cast<uint64_t>(pass_of(phase)) * sectors) + sector;
  const bool counted = mado::check(total > 0U) && mado::check(phase != FormatPhase::kIdle) &&
                       mado::check(sector <= sectors);
  const uint64_t share = counted ? ((done * kFormatPercent) / total) : 0U;
  return static_cast<uint8_t>((share > kFormatPercent) ? kFormatPercent : share);
}

}

#endif
