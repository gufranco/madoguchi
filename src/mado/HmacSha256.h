// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HMAC_SHA256_H
#define MADO_HMAC_SHA256_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "KeyMemory.h"
#include "Sha256.h"
#include "check.h"
#include "kSha256BlockSize.h"
#include "kSha256Size.h"

namespace mado {

class HmacSha256 {
 public:
  bool begin(const uint8_t* const key, const uint8_t length, const KeyMemory memory) {
    keyed_ = false;
    const bool valid = check(key != nullptr) && check(length <= kSha256BlockSize);
    if (!valid) {
      return false;
    }
    const bool inner = absorb_pad(key, length, memory, kInnerPad, &inner_[0]);
    const bool outer = absorb_pad(key, length, memory, kOuterPad, &outer_[0]);
    keyed_ = inner && outer;
    return keyed_ && restart();
  }

  bool restart() {
    const bool ready = check(keyed_);
    return ready && hash_.resume(&inner_[0], kSha256BlockSize);
  }

  bool update(const uint8_t value) {
    const bool ready = check(keyed_);
    return ready && hash_.update(value);
  }

  bool update(const uint8_t* const data, const size_t length) {
    const bool ready = check(keyed_);
    const bool valid = check(data != nullptr) || check(length == 0U);
    return ready && valid && hash_.update(data, length);
  }

  bool finish(uint8_t* const digest) {
    const bool valid = check(digest != nullptr) && check(keyed_);
    if (!valid) {
      return false;
    }
    uint8_t inner_digest[kSha256Size] = {};
    const bool inner_done = hash_.finish(&inner_digest[0]);
    const bool resumed = hash_.resume(&outer_[0], kSha256BlockSize);
    const bool absorbed = hash_.update(&inner_digest[0], sizeof(inner_digest));
    return inner_done && resumed && absorbed && hash_.finish(digest);
  }

 private:
  static constexpr uint8_t kInnerPad = 0x36U;
  static constexpr uint8_t kOuterPad = 0x5CU;

  static uint8_t key_byte(const uint8_t* const key, const uint8_t index, const KeyMemory memory) {
    const bool valid = check(key != nullptr) && check(index < kSha256BlockSize);
    if (!valid) {
      return 0U;
    }
    return (memory == KeyMemory::kFlash) ? pgm_read_byte(key + index) : key[index];
  }

  bool absorb_pad(const uint8_t* const key, const uint8_t length, const KeyMemory memory,
                  const uint8_t pad, uint32_t* const state) {
    const bool valid =
        check(key != nullptr) && check(length <= kSha256BlockSize) && check(state != nullptr);
    if (!valid) {
      return false;
    }
    hash_.reset();
    for (uint8_t i = 0U; i < kSha256BlockSize; ++i) {
      const uint8_t base = (i < length) ? key_byte(key, i, memory) : 0U;
      static_cast<void>(hash_.update(static_cast<uint8_t>(base ^ pad)));
    }
    return hash_.export_state(state);
  }

  Sha256 hash_;
  uint32_t inner_[Sha256::kStateWords] = {};
  uint32_t outer_[Sha256::kStateWords] = {};
  bool keyed_ = false;
};

}

#endif
