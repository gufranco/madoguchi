// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_PATH_ID_H
#define MADO_PARSE_PATH_ID_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kMaxPathIdDigits = 3U;
constexpr uint8_t kMaxPathId = 254U;

inline bool parse_path_id(const char* const path, const char* const flash_prefix,
                          uint8_t& item_id) {
  item_id = 0U;
  const bool valid = check(path != nullptr) && check(flash_prefix != nullptr);
  const size_t prefix_length = valid ? strlen_P(flash_prefix) : 0U;
  const bool prefixed = valid && check(prefix_length < kMaxStringScan) &&
                        (strncmp_P(path, flash_prefix, prefix_length) == 0);
  if (!prefixed) {
    return false;
  }
  const char* const digits = path + prefix_length;
  uint16_t value = 0U;
  uint8_t count = 0U;
  for (; (count <= kMaxPathIdDigits) && (digits[count] >= '0') && (digits[count] <= '9'); ++count) {
    value = static_cast<uint16_t>((value * 10U) + static_cast<uint16_t>(digits[count] - '0'));
  }
  const bool shaped = (count > 0U) && (count <= kMaxPathIdDigits) && (digits[count] == '\0');
  const bool canonical = shaped && (digits[0] != '0');
  if ((!canonical) || (value > kMaxPathId)) {
    return false;
  }
  item_id = static_cast<uint8_t>(value);
  return true;
}

}

#endif
