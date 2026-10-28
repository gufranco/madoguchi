// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_POST_ITEMS_ITEM_CATALOG_SERVICE_H
#define REST_POST_ITEMS_ITEM_CATALOG_SERVICE_H

#include <mado/check.h>
#include <stdint.h>
#include <string.h>

#include "../domain/item.entity.h"
#include "../domain/post_record.value.h"
#include "item_repository.port.h"
#include "post_ledger.port.h"

namespace items {

enum class AddStatus : uint8_t { kAdded, kInvalid, kFull, kReplayed, kKeyReused };

struct AddResult {
  AddStatus status;
  Item item;
};

struct ItemLookup {
  bool found;
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

  template <typename LedgerT>
  static AddResult add_once(RepositoryT& repository, LedgerT& ledger, const char* const key,
                            const ItemDraft& draft) {
    static_assert(PostLedgerPort<LedgerT>::kSatisfied, "the ledger fits its port");
    PostRecord known = {};
    if (!mado::check((key != nullptr) && ledger.recall(key, known))) {
      return remembered(repository, ledger, key, draft);
    }
    if (!mado::check(known.fingerprint == draft_fingerprint(draft))) {
      return AddResult{AddStatus::kKeyReused, Item{}};
    }
    const ItemLookup stored = find(repository, known.item_id);
    return mado::check(stored.found) ? AddResult{AddStatus::kReplayed, stored.item}
                                     : AddResult{AddStatus::kKeyReused, Item{}};
  }

  static ItemLookup find(const RepositoryT& repository, const uint8_t item_id) {
    ItemLookup lookup = {false, Item{}};
    lookup.found = mado::check(is_item_id(item_id)) && repository.find(item_id, lookup.item);
    const bool same = mado::check((!lookup.found) || (lookup.item.id == item_id));
    return same ? lookup : ItemLookup{false, Item{}};
  }

 private:
  template <typename LedgerT>
  static AddResult remembered(RepositoryT& repository, LedgerT& ledger, const char* const key,
                              const ItemDraft& draft) {
    const AddResult added = add(repository, draft);
    const bool keyed = (key != nullptr) && (key[0] != '\0');
    if (!(keyed && mado::check(added.status == AddStatus::kAdded))) {
      return added;
    }
    PostRecord record = {};
    static_cast<void>(strncpy(&record.key[0], key, kKeySize - 1U));
    record.fingerprint = draft_fingerprint(draft);
    record.item_id = added.item.id;
    const bool kept = mado::check(ledger.remember(record));
    static_cast<void>(kept);
    return added;
  }
};

}

#endif
