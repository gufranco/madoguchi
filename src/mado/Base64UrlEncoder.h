// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_BASE64_URL_ENCODER_H
#define MADO_BASE64_URL_ENCODER_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"

namespace mado {

class Base64UrlEncoder {
 public:
  static constexpr uint8_t kMaxOutput = 4U;

  uint8_t push(const uint8_t value, char* const out) {
    const bool valid = check(out != nullptr) && check(pending_ < kGroupBytes);
    const bool narrow = check(group_ < kPendingLimit);
    if (!(valid && narrow)) {
      return 0U;
    }
    group_ = (group_ << 8U) | value;
    pending_ = static_cast<uint8_t>(pending_ + 1U);
    if (pending_ < kGroupBytes) {
      return 0U;
    }
    const uint8_t count = emit(out, kMaxOutput);
    group_ = 0U;
    pending_ = 0U;
    return count;
  }

  uint8_t finish(char* const out) {
    const bool valid = check(out != nullptr) && check(pending_ < kGroupBytes);
    if ((!valid) || (pending_ == 0U)) {
      return 0U;
    }
    const uint8_t missing = static_cast<uint8_t>(kGroupBytes - pending_);
    group_ = group_ << (8U * missing);
    const uint8_t count = emit(out, static_cast<uint8_t>(pending_ + 1U));
    group_ = 0U;
    pending_ = 0U;
    return count;
  }

 private:
  static constexpr uint8_t kGroupBytes = 3U;
  static constexpr uint8_t kSextetMask = 0x3FU;
  static constexpr uint32_t kPendingLimit = 0x10000U;

  static char symbol(const uint8_t index) {
    const bool valid = check(index <= kSextetMask);
    const char* const alphabet =
        PSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
    return valid ? static_cast<char>(pgm_read_byte(alphabet + index)) : 'A';
  }

  uint8_t emit(char* const out, const uint8_t count) const {
    const bool valid = check(out != nullptr) && check(count <= kMaxOutput);
    if (!valid) {
      return 0U;
    }
    for (uint8_t i = 0U; i < count; ++i) {
      const uint8_t shift = static_cast<uint8_t>(18U - (6U * i));
      out[i] = symbol(static_cast<uint8_t>((group_ >> shift) & kSextetMask));
    }
    return count;
  }

  uint32_t group_ = 0U;
  uint8_t pending_ = 0U;
};

}

#endif
