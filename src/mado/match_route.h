// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_MATCH_ROUTE_H
#define MADO_MATCH_ROUTE_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

inline bool match_route(const char* const pattern, const char* const path) {
  const bool valid = check(pattern != nullptr) && check(path != nullptr);
  if (!valid) {
    return false;
  }
  bool matched = false;
  for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
    const char expected = static_cast<char>(pgm_read_byte(pattern + i));
    const char actual = path[i];
    const bool last = (expected != '\0') && (pgm_read_byte(pattern + i + 1U) == 0U);
    const bool wildcard = (expected == '*') && last;
    if (wildcard) {
      matched = true;
      break;
    }
    if (expected != actual) {
      break;
    }
    if (expected == '\0') {
      matched = true;
      break;
    }
  }
  return matched;
}

}

#endif
