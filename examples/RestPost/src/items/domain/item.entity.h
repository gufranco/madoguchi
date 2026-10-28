// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_POST_ITEMS_ITEM_ENTITY_H
#define REST_POST_ITEMS_ITEM_ENTITY_H

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

}

#endif
