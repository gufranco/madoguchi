// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_IDEMPOTENCY_KEY_H
#define MADO_PARSE_IDEMPOTENCY_KEY_H

#include <stdint.h>
#include <string.h>

#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kIdempotencyKeySize = 65U;

inline bool parse_idempotency_key(const char* const header, char (&out)[kIdempotencyKeySize]) {
  out[0] = '\0';
  if (header == nullptr) {
    return false;
  }
  const size_t length = strnlen(header, kMaxStringScan);
  const bool quoted = check(length >= 3U) && (header[0] == '"') && (header[length - 1U] == '"');
  bool clean = quoted && check((length - 2U) < kIdempotencyKeySize);
  for (size_t i = 1U; clean && (i < (length - 1U)); ++i) {
    const char symbol = header[i];
    clean = (symbol >= ' ') && (symbol <= '~') && (symbol != '"') && (symbol != '\\');
  }
  if (!clean) {
    return false;
  }
  static_cast<void>(memcpy(&out[0], &header[1], length - 2U));
  out[length - 2U] = '\0';
  return true;
}

}

#endif
