// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_MEMORY_SOURCE_H
#define MADO_MEMORY_SOURCE_H

#include <stdint.h>

#include "check.h"

namespace mado {

class MemorySource {
 public:
  MemorySource(const uint8_t* const data, const uint16_t length)
      : data_(data), length_((data == nullptr) ? 0U : length) {
    const bool consistent = check((data != nullptr) || (length == 0U));
    length_ = consistent ? length_ : 0U;
  }

  int16_t next() {
    const bool bounded = check(position_ <= length_);
    const bool has_data = check(data_ != nullptr) || check(length_ == 0U);
    if ((!bounded) || (!has_data) || (position_ == length_)) {
      return -1;
    }
    const uint8_t value = data_[position_];
    position_ = static_cast<uint16_t>(position_ + 1U);
    return static_cast<int16_t>(value);
  }

 private:
  const uint8_t* data_;
  uint16_t length_;
  uint16_t position_ = 0U;
};

}

#endif
