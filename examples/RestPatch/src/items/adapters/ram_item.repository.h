// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_ITEMS_RAM_ITEM_REPOSITORY_H
#define REST_PATCH_ITEMS_RAM_ITEM_REPOSITORY_H

#include <mado/SlotTable.h>
#include <mado/check.h>
#include <stdint.h>

#include "../domain/item.entity.h"

namespace items {

class RamItemRepository {
 public:
  uint8_t insert(const ItemDraft& draft) {
    const uint8_t item_id = table_.insert(Item{});
    Item* const stored = mado::check(item_id != 0U) ? table_.find(item_id) : nullptr;
    if (!mado::check(stored != nullptr)) {
      return 0U;
    }
    *stored = item_from(draft, item_id);
    return mado::check(stored->id == item_id) ? item_id : 0U;
  }

  bool find(const uint8_t item_id, Item& out) const {
    const Item* const stored = table_.find(item_id);
    const bool found = mado::check(stored != nullptr) && mado::check(stored->id == item_id);
    out = found ? *stored : Item{};
    return found;
  }

  bool store(const Item& item) {
    Item* const stored = mado::check(is_item_id(item.id)) ? table_.find(item.id) : nullptr;
    if (!mado::check(stored != nullptr)) {
      return false;
    }
    *stored = item;
    return true;
  }

 private:
  mado::SlotTable<Item, kCapacity> table_;
};

}

#endif
