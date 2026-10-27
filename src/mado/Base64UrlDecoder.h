// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_BASE64_URL_DECODER_H
#define MADO_BASE64_URL_DECODER_H

#include <stdint.h>

#include "check.h"

namespace mado {

class Base64UrlDecoder {
 public:
  static constexpr uint8_t kMaxOutput = 3U;

  int8_t push(const char symbol, uint8_t* const out) {
    const bool valid = check(out != nullptr) && check(pending_ < kGroupSymbols);
    const bool narrow = check(group_ < kPendingLimit);
    const uint8_t value = sextet(symbol);
    if ((!valid) || (!narrow) || failed_ || (value == kInvalid)) {
      failed_ = true;
      return -1;
    }
    group_ = (group_ << 6U) | value;
    pending_ = static_cast<uint8_t>(pending_ + 1U);
    if (pending_ < kGroupSymbols) {
      return 0;
    }
    const int8_t count = emit(out, kMaxOutput);
    group_ = 0U;
    pending_ = 0U;
    return count;
  }

  int8_t finish(uint8_t* const out) {
    const bool valid =
        check(out != nullptr) && check(pending_ < kGroupSymbols) && check(group_ < kPendingLimit);
    if ((!valid) || failed_ || (pending_ == 1U)) {
      failed_ = true;
      return -1;
    }
    if (pending_ == 0U) {
      return 0;
    }
    const uint8_t spare_bits = (pending_ == 2U) ? 4U : 2U;
    const uint32_t spare_mask = (static_cast<uint32_t>(1U) << spare_bits) - 1U;
    if ((group_ & spare_mask) != 0U) {
      failed_ = true;
      return -1;
    }
    const uint8_t missing = static_cast<uint8_t>(kGroupSymbols - pending_);
    group_ = group_ << (6U * missing);
    const int8_t count = emit(out, static_cast<uint8_t>(pending_ - 1U));
    pending_ = 0U;
    group_ = 0U;
    return count;
  }

 private:
  static constexpr uint8_t kGroupSymbols = 4U;
  static constexpr uint8_t kInvalid = 0xFFU;
  static constexpr uint32_t kPendingLimit = 0x40000U;

  static uint8_t sextet(const char symbol) {
    const bool present = check(symbol != '\0');
    if (!present) {
      return kInvalid;
    }
    if ((symbol >= 'A') && (symbol <= 'Z')) {
      return static_cast<uint8_t>(symbol - 'A');
    }
    if ((symbol >= 'a') && (symbol <= 'z')) {
      return static_cast<uint8_t>((symbol - 'a') + 26);
    }
    if ((symbol >= '0') && (symbol <= '9')) {
      return static_cast<uint8_t>((symbol - '0') + 52);
    }
    if (symbol == '-') {
      return 62U;
    }
    return (symbol == '_') ? 63U : kInvalid;
  }

  int8_t emit(uint8_t* const out, const uint8_t count) const {
    const bool valid = check(out != nullptr) && check(count <= kMaxOutput);
    if (!valid) {
      return -1;
    }
    for (uint8_t i = 0U; i < count; ++i) {
      const uint8_t shift = static_cast<uint8_t>(16U - (8U * i));
      out[i] = static_cast<uint8_t>(group_ >> shift);
    }
    return static_cast<int8_t>(count);
  }

  uint32_t group_ = 0U;
  uint8_t pending_ = 0U;
  bool failed_ = false;
};

}

#endif
