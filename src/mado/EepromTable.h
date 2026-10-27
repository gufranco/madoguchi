// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_EEPROM_TABLE_H
#define MADO_EEPROM_TABLE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "check.h"

namespace mado {

constexpr uint8_t kEepromTableMagic = 0x7BU;
constexpr uint8_t kEepromTableHeaderBytes = 3U;

template <typename RecordT, uint8_t Capacity, typename StoreT>
class EepromTable {
 public:
  static_assert(Capacity > 0U, "a table needs at least one slot");
  static_assert(Capacity < 0xFFU, "slot ids are 8-bit and 0 means none");
  static_assert(sizeof(RecordT) < 0x100U, "the record size is stored in one byte");
  static_assert((kEepromTableHeaderBytes + (Capacity * (1UL + sizeof(RecordT)))) < 0x10000UL,
                "table offsets are 16-bit");

  static constexpr uint16_t bytes() {
    return static_cast<uint16_t>(kEepromTableHeaderBytes + (Capacity * (1U + sizeof(RecordT))));
  }

  EepromTable(StoreT* const store, const uint16_t base) : store_(store), base_(base) {
  }

  bool open() {
    const bool fits = check(fits_store());
    const bool formatted = fits && (read(0U) == kEepromTableMagic) &&
                           (read(1U) == static_cast<uint8_t>(sizeof(RecordT))) &&
                           (read(2U) == Capacity);
    if (formatted || (!fits)) {
      return formatted;
    }
    write(0U, kEepromTableMagic);
    write(1U, static_cast<uint8_t>(sizeof(RecordT)));
    write(2U, Capacity);
    for (uint8_t i = 0U; i < Capacity; ++i) {
      write(flag_offset(i), 0U);
    }
    const bool stamped = check(read(0U) == kEepromTableMagic) && check(read(2U) == Capacity);
    static_cast<void>(stamped);
    return false;
  }

  uint8_t insert(const RecordT& record) {
    const bool usable = check(fits_store()) && check(read(0U) == kEepromTableMagic);
    for (uint8_t i = 0U; usable && (i < Capacity); ++i) {
      if (read(flag_offset(i)) == 0U) {
        const bool saved = write_record(i, record);
        write(flag_offset(i), saved ? 1U : 0U);
        const bool flagged = check(read(flag_offset(i)) == (saved ? 1U : 0U));
        return (saved && flagged) ? static_cast<uint8_t>(i + 1U) : 0U;
      }
    }
    return 0U;
  }

  bool load(const uint8_t slot, RecordT& record) const {
    if (!used(slot)) {
      return false;
    }
    uint8_t raw[sizeof(RecordT)] = {};
    const uint16_t start = record_offset(static_cast<uint8_t>(slot - 1U));
    for (size_t i = 0U; i < sizeof(RecordT); ++i) {
      raw[i] = read(static_cast<uint16_t>(start + i));
    }
    static_cast<void>(memcpy(&record, &raw[0], sizeof(RecordT)));
    return check(start > kEepromTableHeaderBytes);
  }

  bool save(const uint8_t slot, const RecordT& record) {
    const bool listed = used(slot);
    const bool saved = listed && write_record(static_cast<uint8_t>(slot - 1U), record);
    return saved && check(used(slot));
  }

  bool erase(const uint8_t slot) {
    const bool listed = used(slot);
    if (listed) {
      write(flag_offset(static_cast<uint8_t>(slot - 1U)), 0U);
    }
    return listed && check(!used(slot));
  }

  bool used(const uint8_t slot) const {
    const bool in_range = check(slot <= Capacity) && (slot > 0U);
    const bool usable = in_range && check(fits_store());
    return usable && (read(flag_offset(static_cast<uint8_t>(slot - 1U))) == 1U);
  }

  uint8_t count() const {
    uint8_t used_slots = 0U;
    for (uint8_t slot = 1U; slot <= Capacity; ++slot) {
      used_slots = static_cast<uint8_t>(used_slots + (used(slot) ? 1U : 0U));
    }
    const bool bounded = check(used_slots <= Capacity);
    return bounded ? used_slots : 0U;
  }

  static constexpr uint8_t capacity() {
    return Capacity;
  }

 private:
  static uint16_t flag_offset(const uint8_t index) {
    const bool in_range = check(index < Capacity);
    const uint16_t offset =
        static_cast<uint16_t>(kEepromTableHeaderBytes + (index * (1U + sizeof(RecordT))));
    return in_range ? offset : 0U;
  }

  static uint16_t record_offset(const uint8_t index) {
    const uint16_t flag = flag_offset(index);
    const bool placed = check(flag >= kEepromTableHeaderBytes);
    return placed ? static_cast<uint16_t>(flag + 1U) : kEepromTableHeaderBytes;
  }

  bool fits_store() const {
    const bool wired = check(store_ != nullptr);
    return wired && ((static_cast<uint32_t>(base_) + bytes()) <= store_->length());
  }

  bool write_record(const uint8_t index, const RecordT& record) {
    uint8_t raw[sizeof(RecordT)] = {};
    static_cast<void>(memcpy(&raw[0], &record, sizeof(RecordT)));
    const uint16_t start = record_offset(index);
    bool verified = check(start > kEepromTableHeaderBytes);
    for (size_t i = 0U; i < sizeof(RecordT); ++i) {
      const uint16_t offset = static_cast<uint16_t>(start + i);
      write(offset, raw[i]);
      verified = verified && check(read(offset) == raw[i]);
    }
    return verified;
  }

  uint8_t read(const uint16_t offset) const {
    const bool wired = check(store_ != nullptr) && check(offset < bytes());
    return wired ? store_->read(static_cast<int>(base_ + offset)) : 0U;
  }

  void write(const uint16_t offset, const uint8_t value) {
    const bool wired = check(store_ != nullptr) && check(offset < bytes());
    if (wired) {
      store_->update(static_cast<int>(base_ + offset), value);
    }
  }

  StoreT* store_;
  uint16_t base_;
};

}

#endif
