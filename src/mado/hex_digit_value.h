// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEX_DIGIT_VALUE_H
#define MADO_HEX_DIGIT_VALUE_H

#include <stdint.h>

namespace mado {

constexpr int8_t kNotHex = -1;
constexpr int8_t kFirstHexLetterValue = 10;

inline int8_t hex_digit_value(const char symbol) {
  const bool is_digit = (symbol >= '0') && (symbol <= '9');
  const bool is_lower = (symbol >= 'a') && (symbol <= 'f');
  const bool is_upper = (symbol >= 'A') && (symbol <= 'F');
  int8_t value = kNotHex;
  if (is_digit) {
    value = static_cast<int8_t>(symbol - '0');
  }
  if (is_lower) {
    value = static_cast<int8_t>((symbol - 'a') + kFirstHexLetterValue);
  }
  if (is_upper) {
    value = static_cast<int8_t>((symbol - 'A') + kFirstHexLetterValue);
  }
  return value;
}

}

#endif
