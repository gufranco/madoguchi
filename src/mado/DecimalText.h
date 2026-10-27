// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_DECIMAL_TEXT_H
#define MADO_DECIMAL_TEXT_H

#include <stdint.h>

#include "check.h"

namespace mado {

class DecimalText {
 public:
  explicit DecimalText(const uint32_t value) {
    uint32_t rest = value;
    for (uint8_t i = 0U; i < kCapacity; ++i) {
      const bool has_room = check(start_ > 0U);
      if (!has_room) {
        break;
      }
      --start_;
      digits_[start_] = static_cast<char>('0' + static_cast<char>(rest % kBase));
      rest /= kBase;
      if (rest == 0UL) {
        break;
      }
    }
  }

  const char* begin() const {
    const bool in_range = check(start_ < kCapacity);
    const bool digit = in_range && check(digits_[start_] >= '0') && check(digits_[start_] <= '9');
    return digit ? &digits_[start_] : "0";
  }

  uint8_t length() const {
    const bool in_range = check(start_ < kCapacity);
    const bool digit = in_range && check(digits_[start_] >= '0');
    return digit ? static_cast<uint8_t>(kCapacity - start_) : 1U;
  }

 private:
  static constexpr uint8_t kCapacity = 10U;
  static constexpr uint32_t kBase = 10UL;

  char digits_[kCapacity] = {};
  uint8_t start_ = kCapacity;
};

}

#endif
