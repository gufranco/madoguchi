// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_IDENTITY_EEPROM_SESSION_COUNTER_H
#define REST_API_IDENTITY_EEPROM_SESSION_COUNTER_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/session.value.h"

namespace identity {

constexpr int kSessionAddress = 0;
constexpr uint8_t kSessionBytes = 4U;
constexpr uint32_t kBitsPerByte = 8U;
constexpr uint32_t kByteMask = 0xFFU;

template <typename EepromT>
class EepromSessionCounter {
 public:
  explicit EepromSessionCounter(EepromT& eeprom) : eeprom_(&eeprom) {
  }

  uint32_t advance() {
    const bool fits = mado::check(eeprom_->length() >= (kSessionAddress + kSessionBytes));
    uint32_t stored = 0U;
    for (uint8_t i = 0U; fits && (i < kSessionBytes); ++i) {
      const uint32_t cell = static_cast<uint32_t>(eeprom_->read(kSessionAddress + i));
      stored = stored | (cell << (static_cast<uint32_t>(i) * kBitsPerByte));
    }
    const uint32_t session = next_session(stored);
    for (uint8_t i = 0U; fits && (i < kSessionBytes); ++i) {
      const uint8_t cell =
          static_cast<uint8_t>((session >> (static_cast<uint32_t>(i) * kBitsPerByte)) & kByteMask);
      eeprom_->update(kSessionAddress + i, cell);
    }
    return mado::check(session > 0U) ? session : 1U;
  }

 private:
  EepromT* eeprom_;
};

}

#endif
