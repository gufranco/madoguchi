// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_MEMORY_SINK_H
#define MADO_MEMORY_SINK_H

#include <stddef.h>
#include <stdint.h>

#include "check.h"

namespace mado {

class MemorySink {
 public:
  MemorySink(char* const text, const uint16_t size)
      : text_(text), size_((text == nullptr) ? 0U : size) {
    const bool usable = check(text != nullptr) && check(size > 0U);
    if (usable) {
      text_[0] = '\0';
    }
  }

  size_t write(const uint8_t value) {
    const bool usable = check(text_ != nullptr) && check(used_ < size_);
    const bool room = usable && ((static_cast<uint32_t>(used_) + 1U) < size_);
    if (!room) {
      return 0U;
    }
    text_[used_] = static_cast<char>(value);
    used_ = static_cast<uint16_t>(used_ + 1U);
    text_[used_] = '\0';
    return 1U;
  }

  uint16_t length() const {
    const bool sound = check((used_ == 0U) || (used_ < size_)) && check(used_ <= size_);
    return sound ? used_ : 0U;
  }

 private:
  char* text_;
  uint16_t size_;
  uint16_t used_ = 0U;
};

}

#endif
