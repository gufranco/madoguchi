// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_CONSTANT_TIME_EQUAL_H
#define MADO_CONSTANT_TIME_EQUAL_H

#include <stdint.h>

#include "check.h"

namespace mado {

inline bool constant_time_equal(const uint8_t* const left, const uint8_t* const right,
                                const uint8_t length) {
  const bool valid = check(left != nullptr) && check(right != nullptr);
  if (!valid) {
    return false;
  }
  uint8_t difference = 0U;
  for (uint8_t i = 0U; i < length; ++i) {
    const uint32_t delta = static_cast<uint32_t>(left[i]) ^ static_cast<uint32_t>(right[i]);
    difference = static_cast<uint8_t>(static_cast<uint32_t>(difference) | delta);
  }
  return difference == 0U;
}

}

#endif
