// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ACCEPTS_GZIP_H
#define MADO_ACCEPTS_GZIP_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"
#include "to_lower_ascii.h"

namespace mado {

namespace detail {

constexpr uint8_t kGzipLength = 4U;
static_assert(kGzipLength < kMaxStringScan, "the gzip token must fit the scan bound");

inline bool is_list_space(const char symbol) {
  return (symbol == ' ') || (symbol == '\t');
}

inline bool gzip_at(const char* const text, const uint16_t start) {
  const bool valid = check(text != nullptr) && check(start < kMaxStringScan);
  if (!valid) {
    return false;
  }
  const char* const name = PSTR("gzip");
  bool same = true;
  for (uint8_t i = 0U; i < kGzipLength; ++i) {
    const char symbol = to_lower_ascii(text[start + i]);
    const char expected = static_cast<char>(pgm_read_byte(name + i));
    if (symbol != expected) {
      same = false;
      break;
    }
  }
  return same;
}

inline bool weight_is_zero(const char* const text, const uint16_t start) {
  const bool valid = check(text != nullptr) && check(start < kMaxStringScan);
  if (!valid) {
    return false;
  }
  bool zero = false;
  bool in_weight = false;
  for (uint16_t i = start; i < kMaxStringScan; ++i) {
    const char symbol = to_lower_ascii(text[i]);
    const bool ends = (symbol == '\0') || (symbol == ',');
    if (ends) {
      break;
    }
    if ((symbol == 'q') && (text[i + 1U] == '=')) {
      in_weight = true;
      zero = true;
      continue;
    }
    const bool weight_digit = (symbol == '0') || (symbol == '.') || (symbol == '=');
    zero = zero && ((!in_weight) || weight_digit || is_list_space(symbol));
  }
  return zero;
}

}

inline bool accepts_gzip(const char* const accept_encoding) {
  if (accept_encoding == nullptr) {
    return false;
  }
  bool token_start = true;
  for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
    const char symbol = accept_encoding[i];
    if (symbol == '\0') {
      break;
    }
    if ((symbol == ',') || detail::is_list_space(symbol)) {
      token_start = true;
      continue;
    }
    const bool candidate = token_start && detail::gzip_at(accept_encoding, i);
    token_start = false;
    if (!candidate) {
      continue;
    }
    const char after = accept_encoding[i + detail::kGzipLength];
    const bool whole =
        (after == '\0') || (after == ',') || (after == ';') || detail::is_list_space(after);
    if (whole) {
      return !detail::weight_is_zero(accept_encoding,
                                     static_cast<uint16_t>(i + detail::kGzipLength));
    }
  }
  return false;
}

}

#endif
