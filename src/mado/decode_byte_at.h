// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_DECODE_BYTE_AT_H
#define MADO_DECODE_BYTE_AT_H

#include <stdint.h>

#include "DecodeMode.h"
#include "DecodedByte.h"
#include "check.h"
#include "hex_digit_value.h"

namespace mado {

constexpr uint8_t kHexRadix = 16U;
constexpr uint8_t kEscapeLength = 3U;

inline DecodedByte decode_byte_at(const char* const source, const uint16_t length,
                                  const uint16_t index, const DecodeMode mode) {
  DecodedByte result = {'\0', 1U};
  const bool valid = check(source != nullptr) && check(index < length);
  if (!valid) {
    return result;
  }
  const char current = source[index];
  result.value = current;
  const bool is_plus = (mode == DecodeMode::kForm) && (current == '+');
  if (is_plus) {
    result.value = ' ';
  }
  const uint32_t last_needed = static_cast<uint32_t>(index) + (kEscapeLength - 1U);
  const bool has_room = (current == '%') && (last_needed < length);
  if (!has_room) {
    return result;
  }
  const int8_t high = hex_digit_value(source[index + 1U]);
  const int8_t low = hex_digit_value(source[index + 2U]);
  const bool both_hex = (high >= 0) && (low >= 0);
  if (both_hex) {
    const uint32_t byte_value =
        (static_cast<uint32_t>(high) * kHexRadix) + static_cast<uint32_t>(low);
    result.value = static_cast<char>(byte_value);
    result.consumed = kEscapeLength;
  }
  return result;
}

}

#endif
