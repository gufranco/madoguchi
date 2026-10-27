// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_URL_DECODE_H
#define MADO_URL_DECODE_H

#include <stdint.h>

#include "DecodeMode.h"
#include "DecodedByte.h"
#include "check.h"
#include "decode_byte_at.h"

namespace mado {

constexpr uint16_t kMaxDecodeTarget = 0x7FFFU;

inline int16_t url_decode(const char* const source, const uint16_t source_length,
                          char* const target, const uint16_t target_size, const DecodeMode mode) {
  const bool valid = check(source != nullptr) && check(target != nullptr) &&
                     check(target_size > 0U) && check(target_size <= kMaxDecodeTarget);
  if (!valid) {
    return -1;
  }
  uint16_t written = 0U;
  uint16_t index = 0U;
  for (uint16_t step = 0U; step < source_length; ++step) {
    const bool done = index >= source_length;
    if (done) {
      break;
    }
    const bool full = written >= (target_size - 1U);
    if (full) {
      target[written] = '\0';
      return -1;
    }
    const DecodedByte decoded = decode_byte_at(source, source_length, index, mode);
    target[written] = decoded.value;
    ++written;
    index = static_cast<uint16_t>(index + decoded.consumed);
  }
  target[written] = '\0';
  return static_cast<int16_t>(written);
}

}

#endif
