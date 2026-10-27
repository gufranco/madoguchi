// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FLASH_TEXT_H
#define MADO_FLASH_TEXT_H

#include <Arduino.h>

#include "check.h"

namespace mado {

class FlashText {
 public:
  explicit FlashText(const __FlashStringHelper* const text) : text_(text) {
  }

  const __FlashStringHelper* get() const {
    const bool present = check(text_ != nullptr);
    return present ? text_ : nullptr;
  }

 private:
  const __FlashStringHelper* text_;
};

}

#endif
