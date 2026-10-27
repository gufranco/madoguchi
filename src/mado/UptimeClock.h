// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_UPTIME_CLOCK_H
#define MADO_UPTIME_CLOCK_H

#include <stdint.h>

#include "check.h"

namespace mado {

class UptimeClock {
 public:
  static constexpr uint32_t kMillisPerSecond = 1000U;

  void advance(const uint32_t now_ms) {
    const bool sound = check(remainder_ms_ < kMillisPerSecond);
    const uint32_t elapsed = now_ms - last_ms_;
    last_ms_ = now_ms;
    const uint32_t carried = (sound ? remainder_ms_ : 0U) + (elapsed % kMillisPerSecond);
    const uint32_t before = seconds_;
    seconds_ += (elapsed / kMillisPerSecond) + (carried / kMillisPerSecond);
    remainder_ms_ = carried % kMillisPerSecond;
    const bool forward = check(seconds_ >= before);
    seconds_ = forward ? seconds_ : before;
  }

  uint32_t seconds() const {
    const bool sound = check(remainder_ms_ < kMillisPerSecond);
    return sound ? seconds_ : 0U;
  }

 private:
  uint32_t last_ms_ = 0U;
  uint32_t remainder_ms_ = 0U;
  uint32_t seconds_ = 0U;
};

}

#endif
