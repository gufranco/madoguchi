// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_STORE_H
#define REST_STORE_H

#include <Arduino.h>
#include <mado/FileSink.h>
#include <mado/FileSource.h>
#include <mado/JsonReader.h>
#include <mado/JsonStatus.h>
#include <mado/check.h>
#include <mado/count_file.h>
#include <mado/reference_to.h>
#include <mado/replace_file.h>
#include <stdint.h>
#include <string.h>

#include "generated_api.h"

constexpr uint8_t kStorePathSize = 13U;
constexpr uint8_t kStoreSlots = 4U;
const char kStoreTemporary[] PROGMEM = "/STORE.TMP";
const char kStoreLogin[] PROGMEM = "/LOGIN.TXT";

template <typename SdT, uint8_t kReadMode, uint8_t kWriteMode>
class RestStore {
 public:
  using FileT = decltype(mado::reference_to<SdT>().open("", kReadMode));

  static_assert(kStoreSlots < 10U, "slot numbers are one digit in the file name");

  void attach(SdT* const card) {
    sd_ = card;
  }

  template <typename TableT>
  uint8_t load_items(TableT& items) {
    uint8_t loaded = 0U;
    for (uint8_t slot = 1U; (sd_ != nullptr) && (slot <= kStoreSlots); ++slot) {
      rest_api::Item item = {};
      const bool read = load_item(slot, item);
      loaded = static_cast<uint8_t>(loaded + ((read && items.place(slot, item)) ? 1U : 0U));
    }
    const bool bounded = mado::check(loaded <= kStoreSlots);
    return bounded ? loaded : 0U;
  }

  bool save_item(const rest_api::Item& item) {
    char path[kStorePathSize] = {};
    char temporary[kStorePathSize] = {};
    const bool named =
        (sd_ != nullptr) && item_path(item.id, path) && copy_path(&kStoreTemporary[0], temporary);
    if ((!named) || (!clear(&temporary[0]))) {
      return false;
    }
    FileT file = sd_->open(&temporary[0], kWriteMode);
    if (!static_cast<bool>(file)) {
      return false;
    }
    mado::FileSink<FileT> sink(file);
    const bool written = rest_api::write_item_patch(sink, patch_of(item)) &&
                         (sink.write(static_cast<uint8_t>('\n')) == 1U);
    file.close();
    return written && mado::replace_file<kReadMode, kWriteMode>(*sd_, &temporary[0], &path[0]);
  }

  bool remove_item(const uint8_t slot) {
    char path[kStorePathSize] = {};
    const bool named = (sd_ != nullptr) && item_path(static_cast<int32_t>(slot), path);
    return named && clear(&path[0]);
  }

  uint8_t load_failures() {
    char path[kStorePathSize] = {};
    const bool named = (sd_ != nullptr) && copy_path(&kStoreLogin[0], path);
    return named ? mado::read_count_file<kReadMode>(*sd_, &path[0]) : mado::kNoCount;
  }

  bool save_failures(const uint8_t failures) {
    char path[kStorePathSize] = {};
    char temporary[kStorePathSize] = {};
    const bool named = (sd_ != nullptr) && copy_path(&kStoreLogin[0], path) &&
                       copy_path(&kStoreTemporary[0], temporary);
    return named &&
           mado::write_count_file<kReadMode, kWriteMode>(*sd_, &path[0], &temporary[0], failures);
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

  static bool item_path(const int32_t slot, char (&out)[kStorePathSize]) {
    const bool valid = mado::check(slot > 0) && mado::check(slot <= kStoreSlots);
    static_cast<void>(strncpy_P(&out[0], PSTR("/ITEM0.TXT"), sizeof(out) - 1U));
    out[5] = static_cast<char>('0' + (valid ? slot : 0));
    return valid;
  }

  static rest_api::ItemPatch patch_of(const rest_api::Item& item) {
    static_assert(sizeof(rest_api::ItemPatch::name) == sizeof(rest_api::Item::name),
                  "both schemas share the name limit");
    rest_api::ItemPatch patch = {};
    static_cast<void>(memcpy(&patch.name[0], &item.name[0], sizeof(patch.name)));
    patch.value = item.value;
    patch.on = item.on;
    return patch;
  }

  bool clear(const char* const path) {
    const bool wired = mado::check(sd_ != nullptr) && mado::check(path != nullptr);
    return wired && ((!sd_->exists(path)) || sd_->remove(path));
  }

  bool load_item(const uint8_t slot, rest_api::Item& item) {
    char path[kStorePathSize] = {};
    const bool named = item_path(static_cast<int32_t>(slot), path);
    if ((!named) || (!sd_->exists(&path[0]))) {
      return false;
    }
    FileT file = sd_->open(&path[0], kReadMode);
    mado::FileSource<FileT> source(file);
    mado::JsonReader<mado::FileSource<FileT>> reader(source);
    rest_api::ItemPatch patch = {};
    uint8_t present = 0U;
    const bool parsed =
        static_cast<bool>(file) &&
        (rest_api::read_item_patch(reader, patch, present) == mado::JsonStatus::kOk);
    file.close();
    const bool complete = parsed && (present == rest_api::kItemPatchWritable);
    static_cast<void>(memcpy(&item.name[0], &patch.name[0], sizeof(item.name)));
    item.value = patch.value;
    item.on = patch.on;
    item.id = slot;
    return complete;
  }

  SdT* sd_ = nullptr;
};

#endif
