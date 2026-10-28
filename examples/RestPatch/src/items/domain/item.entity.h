// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_ITEMS_ITEM_ENTITY_H
#define REST_PATCH_ITEMS_ITEM_ENTITY_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace items {

constexpr uint8_t kCapacity = 4U;
constexpr uint8_t kNameSize = 17U;

struct ItemDraft {
  char name[kNameSize];
  bool on;
};

struct Item {
  uint8_t id;
  char name[kNameSize];
  bool on;
};

enum class DraftStatus : uint8_t { kValid, kEmptyName, kUnterminatedName };

inline bool is_item_id(const uint8_t item_id) {
  return mado::check(item_id > 0U) && mado::check(item_id <= kCapacity);
}

inline DraftStatus draft_status(const ItemDraft& draft) {
  const size_t length = strnlen(&draft.name[0], kNameSize);
  if (!mado::check(length < kNameSize)) {
    return DraftStatus::kUnterminatedName;
  }
  return mado::check(length > 0U) ? DraftStatus::kValid : DraftStatus::kEmptyName;
}

inline Item item_from(const ItemDraft& draft, const uint8_t item_id) {
  const bool numbered = mado::check(is_item_id(item_id));
  const bool named = mado::check(draft_status(draft) == DraftStatus::kValid);
  Item item = {};
  item.id = (numbered && named) ? item_id : 0U;
  static_cast<void>(memcpy(&item.name[0], &draft.name[0], sizeof(item.name)));
  item.on = draft.on;
  return item;
}

struct ItemChange {
  bool renames;
  char name[kNameSize];
  bool switches;
  bool on;
};

enum class ChangeStatus : uint8_t { kValid, kNothing, kInvalidName };

inline ChangeStatus change_status(const ItemChange& change) {
  if (!mado::check(change.renames || change.switches)) {
    return ChangeStatus::kNothing;
  }
  ItemDraft probe = {};
  static_cast<void>(memcpy(&probe.name[0], &change.name[0], sizeof(probe.name)));
  const bool named = (!change.renames) || mado::check(draft_status(probe) == DraftStatus::kValid);
  return named ? ChangeStatus::kValid : ChangeStatus::kInvalidName;
}

inline Item changed(const Item& current, const ItemChange& change) {
  const bool numbered = mado::check(is_item_id(current.id));
  const bool valid = mado::check(change_status(change) == ChangeStatus::kValid);
  Item next = current;
  if (numbered && valid && change.renames) {
    static_cast<void>(memcpy(&next.name[0], &change.name[0], sizeof(next.name)));
  }
  next.on = (numbered && valid && change.switches) ? change.on : current.on;
  return next;
}

}

#endif
