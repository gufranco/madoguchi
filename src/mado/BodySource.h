// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_BODY_SOURCE_H
#define MADO_BODY_SOURCE_H

#include <stdint.h>

#include "check.h"

namespace mado {

template <typename ExchangeT>
class BodySource {
 public:
  static constexpr uint8_t kChunkSize = 16U;

  explicit BodySource(ExchangeT& exchange) : exchange_(&exchange) {
  }

  int16_t next() {
    const bool bounded = check(position_ <= fill_) && check(fill_ <= kChunkSize);
    const bool drained = bounded && (position_ == fill_);
    if ((!bounded) || (drained && (!refill()))) {
      return -1;
    }
    const uint8_t value = chunk_[position_];
    position_ = static_cast<uint8_t>(position_ + 1U);
    const bool consumed = check(position_ <= fill_);
    return consumed ? static_cast<int16_t>(value) : -1;
  }

 private:
  bool refill() {
    const bool wired = check(exchange_ != nullptr) && check(position_ == fill_);
    if ((!wired) || exhausted_) {
      return false;
    }
    const int16_t got = exchange_->read_body(&chunk_[0], kChunkSize);
    const bool in_range = check(got <= static_cast<int16_t>(kChunkSize));
    if ((got <= 0) || (!in_range)) {
      exhausted_ = true;
      return false;
    }
    fill_ = static_cast<uint8_t>(got);
    position_ = 0U;
    return true;
  }

  ExchangeT* exchange_;
  uint8_t chunk_[kChunkSize] = {};
  uint8_t position_ = 0U;
  uint8_t fill_ = 0U;
  bool exhausted_ = false;
};

}

#endif
