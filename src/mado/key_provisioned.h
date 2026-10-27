// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_KEY_PROVISIONED_H
#define MADO_KEY_PROVISIONED_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "kTokenKeySize.h"

namespace mado {

inline bool key_provisioned(const uint8_t* const flash_key) {
  const bool present = check(flash_key != nullptr);
  if (!present) {
    return false;
  }
  uint8_t combined = 0U;
  for (uint8_t i = 0U; i < kTokenKeySize; ++i) {
    combined = static_cast<uint8_t>(combined | pgm_read_byte(flash_key + i));
  }
  return combined != 0U;
}

}

#endif
