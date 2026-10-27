// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_GUARDED_BUFFER_H
#define MADO_GUARDED_BUFFER_H

#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint8_t kBarrierLow = 0xA5U;
constexpr uint8_t kBarrierHigh = 0x5AU;

template <typename T, uint16_t N>
class GuardedBuffer {
 public:
  static_assert(N > 0U, "a guarded buffer needs room");
  static_assert(sizeof(T) == 1U, "guarded buffers hold bytes");

  T* data() {
    return &data_[0];
  }

  const T* data() const {
    return &data_[0];
  }

  bool intact() const {
    const bool low_ok = check(barrier_[0] == kBarrierLow);
    const bool high_ok = check(barrier_[1] == kBarrierHigh);
    return low_ok && high_ok;
  }

 private:
  T data_[N] = {};
  uint8_t barrier_[2] = {kBarrierLow, kBarrierHigh};
};

}

#endif
