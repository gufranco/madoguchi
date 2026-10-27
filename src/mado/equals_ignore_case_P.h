// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_EQUALS_IGNORE_CASE_P_H
#define MADO_EQUALS_IGNORE_CASE_P_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"
#include "to_lower_ascii.h"

namespace mado {

inline bool equals_ignore_case_P(const char* const ram, const char* const flash) {
  const bool valid = check(ram != nullptr) && check(flash != nullptr);
  if (!valid) {
    return false;
  }
  const size_t flash_length = strlen_P(flash);
  const bool scannable = check(flash_length < kMaxStringScan);
  if (!scannable) {
    return false;
  }
  bool equal = true;
  for (size_t i = 0U; i < flash_length; ++i) {
    const char ram_symbol = to_lower_ascii(ram[i]);
    const char flash_symbol = to_lower_ascii(static_cast<char>(pgm_read_byte(flash + i)));
    if (ram_symbol != flash_symbol) {
      equal = false;
      break;
    }
  }
  const char ram_after = equal ? ram[flash_length] : '\0';
  return equal && (ram_after == '\0');
}

}

#endif
