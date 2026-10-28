// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_ITEM_ENTITY_H
#define REST_API_ITEMS_ITEM_ENTITY_H

#include <mado/check.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace items {

constexpr uint8_t kCapacity = 4U;
constexpr uint8_t kNameSize = 17U;
constexpr int32_t kMinValue = -100000;
constexpr int32_t kMaxValue = 100000;

struct ItemDraft {
  char name[kNameSize];
  int32_t value;
  bool on;
};

struct Item {
  uint8_t id;
  char name[kNameSize];
  int32_t value;
  bool on;
};

struct ItemChange {
  bool renames;
  char name[kNameSize];
  bool revalues;
  int32_t value;
  bool switches;
  bool on;
};

enum class DraftStatus : uint8_t { kValid, kEmptyName, kUnterminatedName, kValueOutOfRange };

enum class ChangeStatus : uint8_t { kValid, kNothing, kInvalid };

inline bool is_item_id(const uint8_t item_id) {
  return mado::check(item_id > 0U) && mado::check(item_id <= kCapacity);
}

inline DraftStatus draft_status(const ItemDraft& draft) {
  const size_t length = strnlen(&draft.name[0], kNameSize);
  if (!mado::check(length < kNameSize)) {
    return DraftStatus::kUnterminatedName;
  }
  if (!mado::check(length > 0U)) {
    return DraftStatus::kEmptyName;
  }
  const bool in_range =
      mado::check(draft.value >= kMinValue) && mado::check(draft.value <= kMaxValue);
  return in_range ? DraftStatus::kValid : DraftStatus::kValueOutOfRange;
}

inline Item item_from(const ItemDraft& draft, const uint8_t item_id) {
  const bool numbered = mado::check(is_item_id(item_id));
  const bool valid = mado::check(draft_status(draft) == DraftStatus::kValid);
  Item item = {};
  item.id = (numbered && valid) ? item_id : 0U;
  static_cast<void>(memcpy(&item.name[0], &draft.name[0], sizeof(item.name)));
  item.value = draft.value;
  item.on = draft.on;
  return item;
}

inline ItemDraft draft_of(const Item& item, const ItemChange& change) {
  const bool numbered = mado::check(is_item_id(item.id));
  ItemDraft draft = {};
  const char* const name = change.renames ? &change.name[0] : &item.name[0];
  static_cast<void>(memcpy(&draft.name[0], name, sizeof(draft.name)));
  draft.value = change.revalues ? change.value : item.value;
  draft.on = change.switches ? change.on : item.on;
  return mado::check(numbered) ? draft : ItemDraft{};
}

inline ChangeStatus change_status(const Item& item, const ItemChange& change) {
  if (!mado::check(change.renames || change.revalues || change.switches)) {
    return ChangeStatus::kNothing;
  }
  const bool valid = mado::check(draft_status(draft_of(item, change)) == DraftStatus::kValid);
  return valid ? ChangeStatus::kValid : ChangeStatus::kInvalid;
}

}

#endif
