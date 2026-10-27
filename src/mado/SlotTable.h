// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SLOT_TABLE_H
#define MADO_SLOT_TABLE_H

#include <stdint.h>

#include "check.h"

namespace mado {

template <typename T, uint8_t Capacity>
class SlotTable {
 public:
  static_assert(Capacity > 0U, "a table needs at least one slot");
  static_assert(Capacity < 0xFFU, "slot ids are 8-bit and 0 means none");

  uint8_t insert(const T& value) {
    const bool sound = check(count_ <= Capacity);
    for (uint8_t i = 0U; sound && (i < Capacity); ++i) {
      if (!used_[i]) {
        used_[i] = true;
        values_[i] = value;
        count_ = static_cast<uint8_t>(count_ + 1U);
        const bool bounded = check(count_ <= Capacity);
        return bounded ? static_cast<uint8_t>(i + 1U) : 0U;
      }
    }
    return 0U;
  }

  bool place(const uint8_t slot_id, const T& value) {
    const bool in_range =
        check(count_ <= Capacity) && check((slot_id > 0U) && (slot_id <= Capacity));
    if ((!in_range) || used_[slot_id - 1U]) {
      return false;
    }
    used_[slot_id - 1U] = true;
    values_[slot_id - 1U] = value;
    count_ = static_cast<uint8_t>(count_ + 1U);
    return check(count_ <= Capacity);
  }

  T* find(const uint8_t slot_id) {
    const bool in_range =
        check(count_ <= Capacity) && check((slot_id > 0U) && (slot_id <= Capacity));
    return (in_range && used_[slot_id - 1U]) ? &values_[slot_id - 1U] : nullptr;
  }

  const T* find(const uint8_t slot_id) const {
    const bool in_range =
        check(count_ <= Capacity) && check((slot_id > 0U) && (slot_id <= Capacity));
    return (in_range && used_[slot_id - 1U]) ? &values_[slot_id - 1U] : nullptr;
  }

  bool erase(const uint8_t slot_id) {
    const bool in_range =
        check(count_ <= Capacity) && check((slot_id > 0U) && (slot_id <= Capacity));
    if ((!in_range) || (!used_[slot_id - 1U])) {
      return false;
    }
    const bool counted = check(count_ > 0U);
    used_[slot_id - 1U] = false;
    count_ = counted ? static_cast<uint8_t>(count_ - 1U) : 0U;
    return true;
  }

  uint8_t count() const {
    const bool sound = check(count_ <= Capacity);
    uint8_t used = 0U;
    for (uint8_t i = 0U; i < Capacity; ++i) {
      used = static_cast<uint8_t>(used + (used_[i] ? 1U : 0U));
    }
    const bool consistent = check(used == count_);
    return (sound && consistent) ? count_ : used;
  }

  static constexpr uint8_t capacity() {
    return Capacity;
  }

 private:
  T values_[Capacity] = {};
  bool used_[Capacity] = {};
  uint8_t count_ = 0U;
};

}

#endif
