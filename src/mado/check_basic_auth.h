// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_CHECK_BASIC_AUTH_H
#define MADO_CHECK_BASIC_AUTH_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "check.h"
#include "flash_address.h"
#include "kMaxStringScan.h"
#include "to_lower_ascii.h"

namespace mado {

namespace detail {

constexpr uint8_t kBasicSchemeLength = 6U;

inline bool has_basic_scheme(const char* const value) {
  const bool valid = check(value != nullptr);
  if (!valid) {
    return false;
  }
  const char* const scheme = PSTR("basic ");
  bool same = true;
  for (uint8_t i = 0U; i < kBasicSchemeLength; ++i) {
    const char symbol = to_lower_ascii(value[i]);
    const char expected = static_cast<char>(pgm_read_byte(scheme + i));
    if (symbol != expected) {
      same = false;
      break;
    }
  }
  return same;
}

inline uint8_t constant_time_difference(const char* const given, const size_t given_length,
                                        const char* const flash, const size_t flash_length) {
  const bool valid = check(flash != nullptr) && check((given != nullptr) || (given_length == 0U));
  if (!valid) {
    return 1U;
  }
  uint8_t difference = (given_length == flash_length) ? 0U : 1U;
  for (size_t i = 0U; i < flash_length; ++i) {
    const uint8_t expected = pgm_read_byte(flash + i);
    const uint8_t actual = (i < given_length) ? static_cast<uint8_t>(given[i]) : 0U;
    const uint32_t mismatch = static_cast<uint32_t>(expected) ^ static_cast<uint32_t>(actual);
    difference = static_cast<uint8_t>(static_cast<uint32_t>(difference) | mismatch);
  }
  return difference;
}

}

inline bool check_basic_auth(const char* const authorization,
                             const __FlashStringHelper* const expected_credentials) {
  const bool valid = check(expected_credentials != nullptr);
  if ((!valid) || (authorization == nullptr)) {
    return false;
  }
  const bool basic = detail::has_basic_scheme(authorization);
  if (!basic) {
    return false;
  }
  const char* const given = &authorization[detail::kBasicSchemeLength];
  const size_t given_length = strnlen(given, kMaxStringScan);
  const char* const flash = flash_address(expected_credentials);
  const size_t flash_length = strlen_P(flash);
  const bool scannable = check(flash_length < kMaxStringScan);
  if (!scannable) {
    return false;
  }
  const uint8_t difference =
      detail::constant_time_difference(given, given_length, flash, flash_length);
  return difference == 0U;
}

}

#endif
