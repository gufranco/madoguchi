// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_POST_ITEMS_RAM_POST_LEDGER_H
#define REST_POST_ITEMS_RAM_POST_LEDGER_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/post_record.value.h"

namespace items {

constexpr uint8_t kLedgerSize = 8U;

class RamPostLedger {
 public:
  bool recall(const char* const key, PostRecord& out) const {
    const bool given = mado::check(key != nullptr) && mado::check(key[0] != '\0');
    for (uint8_t i = 0U; given && (i < kLedgerSize); ++i) {
      if (same_key(records_[i].key, key)) {
        out = records_[i];
        return true;
      }
    }
    return false;
  }

  bool remember(const PostRecord& record) {
    const bool keyed = mado::check(record.key[0] != '\0');
    const bool slotted = mado::check(next_ < kLedgerSize);
    if (!(keyed && slotted)) {
      return false;
    }
    records_[next_] = record;
    next_ = static_cast<uint8_t>((next_ + 1U) % kLedgerSize);
    return true;
  }

 private:
  PostRecord records_[kLedgerSize] = {};
  uint8_t next_ = 0U;
};

}

#endif
