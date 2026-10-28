// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_FILE_LIBRARY_SERVICE_H
#define REST_FILES_FILES_FILE_LIBRARY_SERVICE_H

#include <Arduino.h>
#include <mado/FileLength.h>
#include <mado/FileStamp.h>
#include <mado/Sha256.h>
#include <mado/check.h>
#include <mado/copy_file.h>
#include <mado/file_length.h>
#include <mado/kSha256Size.h>
#include <mado/valid_card_name.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../domain/file_path.value.h"
#include "../domain/upload.policy.h"
#include "file_card.port.h"

namespace files {

constexpr uint8_t kListMax = 32U;
constexpr uint16_t kChunkSize = 512U;
constexpr uint32_t kMaxReads = kMaxUpload + 1UL;
const char kUploadTemporary[] PROGMEM = "/UPLOAD.TMP";

struct FileEntry {
  char name[mado::kCardNameSize];
  uint64_t size;
  bool directory;
};

enum class ListStatus : uint8_t { kListed, kNoRoot, kCut };
enum class OpenStatus : uint8_t { kOpened, kMissing, kTooLarge };
enum class StoreStatus : uint8_t { kCreated, kReplaced, kIncomplete, kRefused, kDigestMismatch };
enum class RemoveStatus : uint8_t { kRemoved, kMissing, kRefused };

template <typename CardT, typename FileT, uint16_t kReadMode, uint16_t kWriteMode>
class FileLibrary {
 public:
  static_assert(FileCardPort<CardT, FileT>::kSatisfied, "the card fits its port");

  bool begin(const uint8_t chip_select, CardT& card, const uint32_t seed) {
    card_ = &card;
    chip_select_ = chip_select;
    version_ = seed;
    mounted_ = card.begin(chip_select);
    return mado::check(mounted_);
  }

  CardT* mounted() {
    if (!mado::check(card_ != nullptr)) {
      return nullptr;
    }
    if (!mado::check(mounted_)) {
      mounted_ = card_->begin(chip_select_);
      changed(mounted_);
    }
    return mounted_ ? card_ : nullptr;
  }

  template <typename VisitorT>
  ListStatus list(VisitorT& visitor) {
    const char root_path[2] = {'/', '\0'};
    FileT root = mado::check(card_ != nullptr) ? card_->open(&root_path[0], kReadMode) : FileT();
    if (!mado::check(static_cast<bool>(root))) {
      return ListStatus::kNoRoot;
    }
    bool written = visitor.open();
    for (uint8_t i = 0U; written && (i < kListMax); ++i) {
      FileT entry = root.openNextFile();
      if (!entry) {
        break;
      }
      FileEntry item = {};
      const bool named = describe(entry, item);
      entry.close();
      written = (!named) || visitor.visit(item);
    }
    root.close();
    return mado::check(written) ? ListStatus::kListed : ListStatus::kCut;
  }

  OpenStatus open(const FilePath& path, FileT& file, mado::FileStamp& stamp) {
    stamp = mado::FileStamp{0U, version_};
    const bool usable = mado::check(card_ != nullptr) && mado::check(is_file_path(path));
    file = usable ? card_->open(&path.text[0], kReadMode) : FileT();
    if ((!file) || file.isDirectory()) {
      return OpenStatus::kMissing;
    }
    const mado::FileLength length = mado::file_length(file);
    stamp.size = length.bytes;
    return mado::check(length.fits) ? OpenStatus::kOpened : OpenStatus::kTooLarge;
  }

  template <typename SourceT>
  StoreStatus store(const FilePath& path, SourceT& body, const DigestClaim& claim) {
    if (!mado::check(card_ != nullptr)) {
      return StoreStatus::kRefused;
    }
    uint8_t digest[mado::kSha256Size] = {};
    const StoreStatus received = receive(body, digest);
    if (!mado::check(received == StoreStatus::kCreated)) {
      forget_upload();
      return received;
    }
    if (!digest_accepted(claim, digest)) {
      forget_upload();
      return StoreStatus::kDigestMismatch;
    }
    const bool existed = mado::check(is_file_path(path)) && card_->exists(&path.text[0]);
    if (!install(&path.text[0])) {
      return StoreStatus::kRefused;
    }
    return existed ? StoreStatus::kReplaced : StoreStatus::kCreated;
  }

  RemoveStatus remove(const FilePath& path) {
    const bool valid = mado::check(card_ != nullptr) && mado::check(is_file_path(path));
    if (!(valid && card_->exists(&path.text[0]))) {
      return RemoveStatus::kMissing;
    }
    const bool removed = card_->remove(&path.text[0]);
    changed(removed);
    return mado::check(removed) ? RemoveStatus::kRemoved : RemoveStatus::kRefused;
  }

 private:
  void changed(const bool happened) {
    const uint32_t step = mado::check(happened) ? 1U : 0U;
    version_ = version_ + step;
  }

  static bool describe(FileT& entry, FileEntry& item) {
    char name[mado::kCardNameSize + 1U] = {};
    const size_t length = entry.getName(&name[0], sizeof(name));
    const bool named = mado::check(length < sizeof(name)) && (length > 0U) &&
                       (length < sizeof(item.name)) && mado::valid_card_name(&name[0]);
    static_cast<void>(memcpy(&item.name[0], &name[0], named ? length : 0U));
    item.size = entry.size();
    item.directory = entry.isDirectory();
    return mado::check(named);
  }

  static void copy_temporary(char (&out)[kPathSize]) {
    const bool fits = mado::check(strlen_P(&kUploadTemporary[0]) < kPathSize);
    out[0] = '\0';
    if (fits) {
      static_cast<void>(strncpy_P(&out[0], &kUploadTemporary[0], sizeof(out) - 1U));
    }
  }

  template <typename SourceT>
  StoreStatus receive(SourceT& body, uint8_t (&digest)[mado::kSha256Size]) {
    char temporary[kPathSize] = {};
    copy_temporary(temporary);
    if (!mado::check(temporary[0] == '/')) {
      return StoreStatus::kRefused;
    }
    static_cast<void>(card_->remove(&temporary[0]));
    FileT out = card_->open(&temporary[0], kWriteMode);
    if (!mado::check(static_cast<bool>(out))) {
      return StoreStatus::kRefused;
    }
    mado::Sha256 sha;
    uint8_t chunk[kChunkSize] = {};
    bool stored = true;
    for (uint32_t i = 0U; stored && (body.remaining_body() > 0U) && (i < kMaxReads); ++i) {
      const int16_t got = body.read_body(&chunk[0], sizeof(chunk));
      const size_t count = (got > 0) ? static_cast<size_t>(got) : 0U;
      stored = (got > 0) && (out.write(&chunk[0], count) == count) && sha.update(&chunk[0], count);
    }
    out.close();
    const bool complete = stored && (body.remaining_body() == 0U) && sha.finish(&digest[0]);
    if (mado::check(complete)) {
      return StoreStatus::kCreated;
    }
    return stored ? StoreStatus::kIncomplete : StoreStatus::kRefused;
  }

  void forget_upload() {
    char temporary[kPathSize] = {};
    copy_temporary(temporary);
    const bool removed = mado::check(temporary[0] == '/') && card_->remove(&temporary[0]);
    static_cast<void>(removed);
  }

  bool install(const char* const path) {
    char temporary[kPathSize] = {};
    copy_temporary(temporary);
    if (!mado::check(temporary[0] == '/')) {
      return false;
    }
    const bool existed = mado::check(path != nullptr) && card_->exists(path);
    const bool cleared = (!existed) || card_->remove(path);
    FileT from = card_->open(&temporary[0], kReadMode);
    FileT target = card_->open(path, kWriteMode);
    const bool opened = cleared && static_cast<bool>(from) && static_cast<bool>(target);
    const mado::FileLength length = opened ? mado::file_length(from) : mado::FileLength{false, 0U};
    const bool copied = length.fits && mado::copy_file(from, target, length.bytes);
    from.close();
    target.close();
    static_cast<void>(card_->remove(&temporary[0]));
    changed(existed || copied);
    return mado::check(copied);
  }

  CardT* card_ = nullptr;
  uint8_t chip_select_ = 0U;
  uint32_t version_ = 0U;
  bool mounted_ = false;
};

}

#endif
