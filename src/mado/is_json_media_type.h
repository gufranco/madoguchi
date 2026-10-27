// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_IS_JSON_MEDIA_TYPE_H
#define MADO_IS_JSON_MEDIA_TYPE_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "to_lower_ascii.h"

namespace mado {

constexpr uint8_t kJsonMediaTypeLength = 16U;

inline bool is_json_media_type(const char* const content_type) {
  const bool present = check(content_type != nullptr);
  if (!present) {
    return false;
  }
  const char* const expected = PSTR("application/json");
  bool same = true;
  uint8_t matched = 0U;
  for (; same && (matched <= kJsonMediaTypeLength); ++matched) {
    const char wanted = static_cast<char>(pgm_read_byte(expected + matched));
    if (wanted == '\0') {
      break;
    }
    same = to_lower_ascii(content_type[matched]) == wanted;
  }
  same = same && check(matched == kJsonMediaTypeLength);
  const char after = same ? content_type[kJsonMediaTypeLength] : '\0';
  const bool ended = (after == '\0') || (after == ';') || (after == ' ') || (after == '\t');
  return same && ended;
}

}

#endif
