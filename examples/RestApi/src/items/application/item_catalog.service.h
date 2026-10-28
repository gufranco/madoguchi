// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_ITEM_CATALOG_SERVICE_H
#define REST_API_ITEMS_ITEM_CATALOG_SERVICE_H

#include <mado/check.h>
#include <stdint.h>

#include "../domain/item.entity.h"
#include "item_repository.port.h"

namespace items {

enum class Outcome : uint8_t { kDone, kMissing, kInvalid, kNothing, kFull };

struct Result {
  Outcome outcome;
  Item item;
  bool saved;
};

struct ItemLookup {
  bool found;
  Item item;
};

template <typename RepositoryT>
class ItemCatalog {
 public:
  static_assert(ItemRepositoryPort<RepositoryT>::kSatisfied, "the repository fits its port");

  static Result add(RepositoryT& repository, const ItemDraft& draft) {
    if (!mado::check(draft_status(draft) == DraftStatus::kValid)) {
      return Result{Outcome::kInvalid, Item{}, true};
    }
    uint8_t item_id = 0U;
    const Written written = repository.insert(draft, item_id);
    const ItemLookup stored = mado::check(written.done) ? find(repository, item_id) : ItemLookup{};
    return mado::check(stored.found) ? Result{Outcome::kDone, stored.item, written.saved}
                                     : Result{Outcome::kFull, Item{}, true};
  }

  static ItemLookup find(const RepositoryT& repository, const uint8_t item_id) {
    ItemLookup lookup = {false, Item{}};
    lookup.found = mado::check(is_item_id(item_id)) && repository.find(item_id, lookup.item);
    const bool same = mado::check((!lookup.found) || (lookup.item.id == item_id));
    return same ? lookup : ItemLookup{false, Item{}};
  }

  static Result replace(RepositoryT& repository, const uint8_t item_id, const ItemDraft& draft) {
    const ItemLookup current = find(repository, item_id);
    if (!mado::check(current.found)) {
      return Result{Outcome::kMissing, Item{}, true};
    }
    if (!mado::check(draft_status(draft) == DraftStatus::kValid)) {
      return Result{Outcome::kInvalid, current.item, true};
    }
    return keep(repository, item_from(draft, item_id));
  }

  static Result change(RepositoryT& repository, const uint8_t item_id,
                       const ItemChange& requested) {
    const ItemLookup current = find(repository, item_id);
    if (!mado::check(current.found)) {
      return Result{Outcome::kMissing, Item{}, true};
    }
    const ChangeStatus status = change_status(current.item, requested);
    if (!mado::check(status == ChangeStatus::kValid)) {
      const bool nothing = status == ChangeStatus::kNothing;
      return Result{nothing ? Outcome::kNothing : Outcome::kInvalid, current.item, true};
    }
    return keep(repository, item_from(draft_of(current.item, requested), item_id));
  }

  static Result remove(RepositoryT& repository, const uint8_t item_id) {
    const bool numbered = mado::check(is_item_id(item_id));
    const Written written = numbered ? repository.erase(item_id) : Written{false, true};
    return mado::check(written.done) ? Result{Outcome::kDone, Item{}, written.saved}
                                     : Result{Outcome::kMissing, Item{}, true};
  }

 private:
  static Result keep(RepositoryT& repository, const Item& next) {
    const bool numbered = mado::check(is_item_id(next.id));
    const Written written = numbered ? repository.store(next) : Written{false, true};
    return mado::check(written.done) ? Result{Outcome::kDone, next, written.saved}
                                     : Result{Outcome::kMissing, Item{}, true};
  }
};

}

#endif
