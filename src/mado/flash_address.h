// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FLASH_ADDRESS_H
#define MADO_FLASH_ADDRESS_H

#include <Arduino.h>

#include "check.h"

namespace mado {

inline const char* flash_address(const __FlashStringHelper* const text) {
  const bool valid = check(text != nullptr);
  return valid ? reinterpret_cast<const char*>(text) : PSTR("");
}

}

#endif
