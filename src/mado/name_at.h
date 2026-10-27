// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_NAME_AT_H
#define MADO_NAME_AT_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"

namespace mado {

inline const char* name_at(const char* const* const table, const uint8_t index) {
  const char* name = nullptr;
  const bool valid = check(table != nullptr) && check(index < 0xFFU);
  if (valid) {
    static_cast<void>(
        memcpy_P(static_cast<void*>(&name), static_cast<const void*>(&table[index]), sizeof(name)));
  }
  return name;
}

}

#endif
