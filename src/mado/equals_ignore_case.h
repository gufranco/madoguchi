// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_EQUALS_IGNORE_CASE_H
#define MADO_EQUALS_IGNORE_CASE_H

#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"
#include "to_lower_ascii.h"

namespace mado {

inline bool equals_ignore_case(const char* const left, const char* const right) {
  const bool valid = check(left != nullptr) && check(right != nullptr);
  if (!valid) {
    return false;
  }
  bool equal = false;
  for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
    const char left_symbol = to_lower_ascii(left[i]);
    const char right_symbol = to_lower_ascii(right[i]);
    if (left_symbol != right_symbol) {
      break;
    }
    if (left_symbol == '\0') {
      equal = true;
      break;
    }
  }
  return equal;
}

}

#endif
