// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PUT_ITEMS_ITEM_CATALOG_SERVICE_H
#define REST_PUT_ITEMS_ITEM_CATALOG_SERVICE_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/item.entity.h"
#include "item_repository.port.h"

namespace items {

enum class AddStatus : uint8_t { kAdded, kInvalid, kFull };

struct AddResult {
  AddStatus status;
  Item item;
};

struct ItemLookup {
  bool found;
  Item item;
};

enum class ReplaceStatus : uint8_t { kReplaced, kMissing, kInvalid };

struct ReplaceResult {
  ReplaceStatus status;
  Item item;
};

template <typename RepositoryT>
class ItemCatalog {
 public:
  static_assert(ItemRepositoryPort<RepositoryT>::kSatisfied, "the repository fits its port");

  static AddResult add(RepositoryT& repository, const ItemDraft& draft) {
    if (!mado::check(draft_status(draft) == DraftStatus::kValid)) {
      return AddResult{AddStatus::kInvalid, Item{}};
    }
    const uint8_t item_id = repository.insert(draft);
    if (!mado::check(item_id != 0U)) {
      return AddResult{AddStatus::kFull, Item{}};
    }
    const ItemLookup stored = find(repository, item_id);
    return mado::check(stored.found) ? AddResult{AddStatus::kAdded, stored.item}
                                     : AddResult{AddStatus::kFull, Item{}};
  }

  static ItemLookup find(const RepositoryT& repository, const uint8_t item_id) {
    ItemLookup lookup = {false, Item{}};
    lookup.found = mado::check(is_item_id(item_id)) && repository.find(item_id, lookup.item);
    const bool same = mado::check((!lookup.found) || (lookup.item.id == item_id));
    return same ? lookup : ItemLookup{false, Item{}};
  }

  static ReplaceResult replace(RepositoryT& repository, const uint8_t item_id,
                               const ItemDraft& draft) {
    const ItemLookup current = find(repository, item_id);
    if (!mado::check(current.found)) {
      return ReplaceResult{ReplaceStatus::kMissing, Item{}};
    }
    if (!mado::check(draft_status(draft) == DraftStatus::kValid)) {
      return ReplaceResult{ReplaceStatus::kInvalid, current.item};
    }
    const Item next = replaced(current.item, draft);
    return mado::check(repository.store(next)) ? ReplaceResult{ReplaceStatus::kReplaced, next}
                                               : ReplaceResult{ReplaceStatus::kMissing, Item{}};
  }
};

}

#endif
