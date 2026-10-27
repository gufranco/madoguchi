// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_TOKEN_HEADER_SEGMENT_H
#define MADO_TOKEN_HEADER_SEGMENT_H

#include <Arduino.h>
#include <stdint.h>

namespace mado {

constexpr uint8_t kTokenHeaderLength = 36U;

inline const char* token_header_segment() {
  return PSTR("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9");
}

}

#endif
