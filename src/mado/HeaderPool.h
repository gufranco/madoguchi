// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEADER_POOL_H
#define MADO_HEADER_POOL_H

#include <stdint.h>

#include "GuardedBuffer.h"
#include "check.h"

namespace mado {

constexpr uint8_t kNoSlot = 0xFFU;

template <typename LimitsT>
class HeaderPool {
 public:
  static_assert(LimitsT::kHeaderPoolSize < kNoSlot, "pool offsets are 8-bit");
  static_assert(LimitsT::kMaxCapturedHeaders < kNoSlot, "slot ids are 8-bit");
  static_assert(LimitsT::kHeaderPoolSize > 1U, "the pool must hold a terminator");

  HeaderPool() {
    reset();
  }

  void reset() {
    for (uint8_t i = 0U; i < LimitsT::kMaxCapturedHeaders; ++i) {
      offsets_[i] = kNoSlot;
    }
    const bool barrier_ok = check(storage_.intact());
    used_ = barrier_ok ? 0U : LimitsT::kHeaderPoolSize;
    current_ = kNoSlot;
  }

  bool begin(const uint8_t slot) {
    const bool valid = check(slot < LimitsT::kMaxCapturedHeaders) && check(current_ == kNoSlot);
    const bool has_room = used_ < LimitsT::kHeaderPoolSize;
    if (!(valid && has_room)) {
      return false;
    }
    offsets_[slot] = used_;
    current_ = slot;
    return true;
  }

  void append(const char symbol) {
    const bool bounded = check(used_ <= LimitsT::kHeaderPoolSize);
    const bool open = bounded && check(current_ != kNoSlot);
    const bool has_room = (static_cast<uint16_t>(used_) + 1U) < LimitsT::kHeaderPoolSize;
    if (open && has_room) {
      storage_.data()[used_] = symbol;
      ++used_;
    }
  }

  void finish() {
    const bool open = check(current_ != kNoSlot);
    const bool in_bounds = check(used_ < LimitsT::kHeaderPoolSize);
    if (open && in_bounds) {
      storage_.data()[used_] = '\0';
      ++used_;
    }
    current_ = kNoSlot;
  }

  const char* value(const uint8_t slot) const {
    const bool valid = check(slot < LimitsT::kMaxCapturedHeaders);
    if (!valid) {
      return nullptr;
    }
    const uint8_t offset = offsets_[slot];
    const bool offset_ok = check((offset == kNoSlot) || (offset < LimitsT::kHeaderPoolSize));
    const bool captured = offset_ok && (offset != kNoSlot) && (current_ != slot);
    return captured ? &storage_.data()[offset] : nullptr;
  }

  bool intact() const {
    const bool barrier_ok = check(storage_.intact());
    const bool bounded = check(used_ <= LimitsT::kHeaderPoolSize);
    return barrier_ok && bounded;
  }

 private:
  GuardedBuffer<char, LimitsT::kHeaderPoolSize> storage_;
  uint8_t offsets_[LimitsT::kMaxCapturedHeaders] = {};
  uint8_t used_ = 0U;
  uint8_t current_ = kNoSlot;
};

}

#endif
