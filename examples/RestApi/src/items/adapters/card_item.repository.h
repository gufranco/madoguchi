// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_CARD_ITEM_REPOSITORY_H
#define REST_API_ITEMS_CARD_ITEM_REPOSITORY_H

#include <Arduino.h>
#include <mado/FileSink.h>
#include <mado/FileSource.h>
#include <mado/JsonReader.h>
#include <mado/JsonStatus.h>
#include <mado/SlotTable.h>
#include <mado/check.h>
#include <mado/reference_to.h>
#include <mado/replace_file.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/item_repository.port.h"
#include "../domain/item.entity.h"

namespace items {

constexpr uint8_t kStorePathSize = 13U;
constexpr uint8_t kSlotDigit = 5U;
const char kStoreTemporary[] PROGMEM = "/STORE.TMP";
const char kItemPathPattern[] PROGMEM = "/ITEM0.TXT";

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class CardItemRepository {
 public:
  using FileT = decltype(mado::reference_to<SdT>().open("", kReadMode));

  static_assert(kCapacity < 10U, "item numbers are one digit in the file name");
  static_assert(sizeof(rest_api::ItemPatch::name) == kNameSize, "the stored schema agrees");

  uint8_t attach(SdT* const card) {
    card_ = card;
    table_ = mado::SlotTable<Item, kCapacity>{};
    uint8_t loaded = 0U;
    for (uint8_t item_id = 1U; (card_ != nullptr) && (item_id <= kCapacity); ++item_id) {
      const Item item = load(item_id);
      const bool placed = mado::check(item.id == item_id) && table_.place(item_id, item);
      loaded = static_cast<uint8_t>(loaded + (placed ? 1U : 0U));
    }
    return mado::check(loaded <= kCapacity) ? loaded : 0U;
  }

  Written insert(const ItemDraft& draft, uint8_t& item_id) {
    item_id = table_.insert(Item{});
    Item* const stored = mado::check(item_id != 0U) ? table_.find(item_id) : nullptr;
    if (!mado::check(stored != nullptr)) {
      return Written{false, true};
    }
    *stored = item_from(draft, item_id);
    return Written{true, save(*stored)};
  }

  bool find(const uint8_t item_id, Item& out) const {
    const Item* const stored = table_.find(item_id);
    const bool found = mado::check(stored != nullptr) && mado::check(stored->id == item_id);
    out = found ? *stored : Item{};
    return found;
  }

  Written store(const Item& item) {
    Item* const stored = mado::check(is_item_id(item.id)) ? table_.find(item.id) : nullptr;
    if (!mado::check(stored != nullptr)) {
      return Written{false, true};
    }
    *stored = item;
    return Written{true, save(item)};
  }

  Written erase(const uint8_t item_id) {
    const bool erased = mado::check(is_item_id(item_id)) && table_.erase(item_id);
    char path[kStorePathSize] = {};
    const bool named = mado::check(item_path(item_id, path));
    const bool forgotten = (card_ == nullptr) || (named && clear(&path[0]));
    return Written{erased, forgotten};
  }

 private:
  static bool copy_path(const char* const flash, char (&out)[kStorePathSize]) {
    const bool valid =
        mado::check(flash != nullptr) && mado::check(strlen_P(flash) < kStorePathSize);
    out[0] = '\0';
    if (valid) {
      static_cast<void>(strncpy_P(&out[0], flash, sizeof(out) - 1U));
    }
    return valid;
  }

  static bool item_path(const uint8_t item_id, char (&out)[kStorePathSize]) {
    const bool valid = mado::check(is_item_id(item_id)) && copy_path(&kItemPathPattern[0], out);
    const bool patterned = mado::check(out[kSlotDigit] == '0');
    out[kSlotDigit] = static_cast<char>('0' + ((valid && patterned) ? item_id : 0U));
    return valid && patterned;
  }

  bool clear(const char* const path) {
    const bool wired = mado::check(card_ != nullptr) && mado::check(path != nullptr);
    return wired && ((!card_->exists(path)) || card_->remove(path));
  }

  bool save(const Item& item) {
    char path[kStorePathSize] = {};
    char temporary[kStorePathSize] = {};
    if (card_ == nullptr) {
      return true;
    }
    const bool named = item_path(item.id, path) && copy_path(&kStoreTemporary[0], temporary);
    if (!(mado::check(named) && clear(&temporary[0]))) {
      return false;
    }
    FileT file = card_->open(&temporary[0], kWriteMode);
    if (!mado::check(static_cast<bool>(file))) {
      return false;
    }
    mado::FileSink<FileT> sink(file);
    const bool written = rest_api::write_item_patch(sink, patch_of(item)) &&
                         (sink.write(static_cast<uint8_t>('\n')) == 1U);
    file.close();
    return written && mado::replace_file<kReadMode, kWriteMode>(*card_, &temporary[0], &path[0]);
  }

  static rest_api::ItemPatch patch_of(const Item& item) {
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0') &&
                       mado::check((item.value >= kMinValue) && (item.value <= kMaxValue));
    rest_api::ItemPatch patch = {};
    static_cast<void>(memcpy(&patch.name[0], &item.name[0], named ? sizeof(patch.name) : 0U));
    patch.value = item.value;
    patch.on = item.on;
    return patch;
  }

  Item load(const uint8_t item_id) {
    char path[kStorePathSize] = {};
    if (!(mado::check(item_path(item_id, path)) && card_->exists(&path[0]))) {
      return Item{};
    }
    FileT file = card_->open(&path[0], kReadMode);
    mado::FileSource<FileT> source(file);
    mado::JsonReader<mado::FileSource<FileT>> reader(source);
    rest_api::ItemPatch patch = {};
    uint8_t present = 0U;
    const bool parsed =
        static_cast<bool>(file) &&
        (rest_api::read_item_patch(reader, patch, present) == mado::JsonStatus::kOk);
    file.close();
    ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], &patch.name[0], sizeof(draft.name)));
    draft.value = patch.value;
    draft.on = patch.on;
    const bool complete = mado::check(parsed) && (present == rest_api::kItemPatchWritable);
    return complete ? item_from(draft, item_id) : Item{};
  }

  SdT* card_ = nullptr;
  mado::SlotTable<Item, kCapacity> table_;
};

}

#endif
