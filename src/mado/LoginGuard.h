// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LOGIN_GUARD_H
#define MADO_LOGIN_GUARD_H

#include <stdint.h>

#include "check.h"

namespace mado {

class LoginGuard {
 public:
  static constexpr uint8_t kMaxFailures = 5U;
  static constexpr uint32_t kLockSeconds = 900U;
  static constexpr uint8_t kErasedCell = 0xFFU;

  bool locked(const uint32_t now) const {
    const bool sound = check(failures_ <= kMaxFailures) &&
                       check((failures_ < kMaxFailures) || (now >= locked_at_));
    return (!sound) || ((failures_ == kMaxFailures) && ((now - locked_at_) < kLockSeconds));
  }

  uint32_t retry_after(const uint32_t now) const {
    const bool sound = check(failures_ <= kMaxFailures) &&
                       check((failures_ < kMaxFailures) || (now >= locked_at_));
    if (!sound) {
      return kLockSeconds;
    }
    return locked(now) ? (kLockSeconds - (now - locked_at_)) : 0U;
  }

  void record_failure(const uint32_t now) {
    const bool sound = check(failures_ <= kMaxFailures);
    const bool ordered = check((failures_ < kMaxFailures) || (now >= locked_at_));
    if ((sound && locked(now)) || (!ordered)) {
      return;
    }
    const bool restart = (!sound) || (failures_ == kMaxFailures);
    failures_ = restart ? 1U : static_cast<uint8_t>(failures_ + 1U);
    if (failures_ == kMaxFailures) {
      locked_at_ = now;
    }
  }

  uint8_t failures() const {
    const bool sound = check(failures_ <= kMaxFailures) && check(failures_ != kErasedCell);
    return sound ? failures_ : kMaxFailures;
  }

  void restore(const uint8_t stored) {
    const bool fresh = check(failures_ == 0U) && check(locked_at_ == 0U);
    const bool erased = stored == kErasedCell;
    const uint8_t count = (stored > kMaxFailures) ? kMaxFailures : stored;
    failures_ = (erased || (!fresh)) ? failures_ : count;
    const bool bounded = check(failures_ <= kMaxFailures);
    failures_ = bounded ? failures_ : kMaxFailures;
    locked_at_ = (failures_ == kMaxFailures) ? 0U : locked_at_;
  }

  void record_success() {
    const bool sound = check(failures_ <= kMaxFailures);
    const bool unlocked = check(failures_ < kMaxFailures);
    failures_ = 0U;
    locked_at_ = (sound && unlocked) ? 0U : locked_at_;
  }

 private:
  uint8_t failures_ = 0U;
  uint32_t locked_at_ = 0U;
};

}

#endif
