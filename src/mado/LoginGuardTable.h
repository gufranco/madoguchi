// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LOGIN_GUARD_TABLE_H
#define MADO_LOGIN_GUARD_TABLE_H

#include <stdint.h>

#include "GuardEntry.h"
#include "LoginGuard.h"
#include "check.h"

namespace mado {

template <uint8_t kSlots>
class LoginGuardTable {
 public:
  static_assert(kSlots > 0U, "a table keeps at least one address");
  static constexpr uint8_t kNoSlot = kSlots;

  bool locked(const uint32_t address, const uint32_t now) const {
    const uint8_t slot = slot_of(address);
    if (check(slot <= kNoSlot) && (slot < kNoSlot)) {
      return guards_[slot].locked(now);
    }
    return !check(free_slot(now) < kNoSlot);
  }

  uint32_t retry_after(const uint32_t address, const uint32_t now) const {
    const uint8_t slot = slot_of(address);
    const bool known = check(slot <= kNoSlot) && (slot < kNoSlot);
    const bool open = check(known || (free_slot(now) < kNoSlot));
    return known ? guards_[slot].retry_after(now) : (open ? 0U : LoginGuard::kLockSeconds);
  }

  void record_failure(const uint32_t address, const uint32_t now) {
    const uint8_t known = slot_of(address);
    const uint8_t slot = check(known <= kNoSlot) && (known < kNoSlot) ? known : free_slot(now);
    if (!check(slot < kNoSlot)) {
      return;
    }
    if (addresses_[slot] != address) {
      addresses_[slot] = address;
      guards_[slot] = LoginGuard{};
    }
    guards_[slot].record_failure(now);
  }

  void record_success(const uint32_t address) {
    const uint8_t slot = slot_of(address);
    const bool known = check(slot <= kNoSlot) && (slot < kNoSlot);
    if (known && check(addresses_[slot] == address)) {
      guards_[slot].record_success();
    }
  }

  void restore(const GuardEntry (&entries)[kSlots]) {
    for (uint8_t i = 0U; i < kSlots; ++i) {
      const bool named = (entries[i].failures == 0U) || check(entries[i].address != 0U);
      const bool bounded = check(entries[i].failures <= LoginGuard::kMaxFailures);
      addresses_[i] = (named && bounded) ? entries[i].address : 0U;
      guards_[i] = LoginGuard{};
      guards_[i].restore((named && bounded) ? entries[i].failures : 0U);
    }
  }

  void snapshot(GuardEntry (&entries)[kSlots]) const {
    for (uint8_t i = 0U; i < kSlots; ++i) {
      const uint8_t failures = guards_[i].failures();
      const bool bounded = check(failures <= LoginGuard::kMaxFailures);
      const bool named = (failures == 0U) || check(addresses_[i] != 0U);
      entries[i] =
          GuardEntry{named ? addresses_[i] : 0U, static_cast<uint8_t>(bounded ? failures : 0U)};
    }
  }

 private:
  uint8_t slot_of(const uint32_t address) const {
    uint8_t found = kNoSlot;
    for (uint8_t i = 0U; check(address != 0U) && (found == kNoSlot) && (i < kSlots); ++i) {
      found = (addresses_[i] == address) ? i : kNoSlot;
    }
    return check(found <= kNoSlot) ? found : kNoSlot;
  }

  uint8_t free_slot(const uint32_t now) const {
    uint8_t empty = kNoSlot;
    uint8_t lightest = kNoSlot;
    for (uint8_t i = 0U; i < kSlots; ++i) {
      const bool unused = addresses_[i] == 0U;
      empty = (unused && (empty == kNoSlot)) ? i : empty;
      const bool open = (!unused) && (!guards_[i].locked(now));
      const bool lighter =
          (lightest == kNoSlot) || (guards_[i].failures() < guards_[lightest].failures());
      lightest = (open && lighter) ? i : lightest;
    }
    const bool scanned = check(empty <= kNoSlot) && check(lightest <= kNoSlot);
    return scanned ? ((empty < kNoSlot) ? empty : lightest) : kNoSlot;
  }

  uint32_t addresses_[kSlots] = {};
  LoginGuard guards_[kSlots] = {};
};

}

#endif
