// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_CARD_POST_LEDGER_H
#define REST_API_ITEMS_CARD_POST_LEDGER_H

#include <Arduino.h>
#include <mado/FileSink.h>
#include <mado/FileSource.h>
#include <mado/check.h>
#include <mado/reference_to.h>
#include <mado/replace_file.h>
#include <stdint.h>
#include <string.h>

#include "../domain/item.entity.h"
#include "../domain/post_record.value.h"

namespace items {

constexpr uint8_t kLedgerSize = 8U;
constexpr uint8_t kFingerprintDigits = 8U;
constexpr uint8_t kLedgerLineSize = kFingerprintDigits + 3U + kKeySize;
constexpr uint8_t kLedgerPathSize = 11U;
const char kLedgerPath[] PROGMEM = "/KEYS.TXT";
const char kLedgerTemporary[] PROGMEM = "/KEYS.TMP";

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class CardPostLedger {
 public:
  using FileT = decltype(mado::reference_to<SdT>().open("", kReadMode));

  uint8_t attach(SdT* const card) {
    card_ = card;
    next_ = 0U;
    static_cast<void>(memset(&records_[0], 0, sizeof(records_)));
    char path[kLedgerPathSize] = {};
    const bool named = mado::check(copy_path(&kLedgerPath[0], path));
    if (!(named && (card_ != nullptr) && card_->exists(&path[0]))) {
      return 0U;
    }
    FileT file = card_->open(&path[0], kReadMode);
    mado::FileSource<FileT> source(file);
    uint8_t loaded = 0U;
    for (uint8_t i = 0U; static_cast<bool>(file) && (i < kLedgerSize); ++i) {
      char line[kLedgerLineSize] = {};
      const bool parsed = read_line(source, line) && parse_record(&line[0], records_[loaded]);
      loaded = static_cast<uint8_t>(loaded + (parsed ? 1U : 0U));
    }
    file.close();
    next_ = static_cast<uint8_t>(loaded % kLedgerSize);
    return mado::check(loaded <= kLedgerSize) ? loaded : 0U;
  }

  bool recall(const char* const key, PostRecord& out) const {
    const bool given = mado::check(key != nullptr) && mado::check(key[0] != '\0');
    for (uint8_t i = 0U; given && (i < kLedgerSize); ++i) {
      if (same_key(records_[i].key, key)) {
        out = records_[i];
        return true;
      }
    }
    return false;
  }

  bool remember(const PostRecord& record) {
    const bool keyed = mado::check(record.key[0] != '\0');
    const bool slotted = mado::check(next_ < kLedgerSize);
    if (!(keyed && slotted)) {
      return false;
    }
    records_[next_] = record;
    next_ = static_cast<uint8_t>((next_ + 1U) % kLedgerSize);
    return (card_ == nullptr) || save();
  }

 private:
  static bool copy_path(const char* const flash, char (&out)[kLedgerPathSize]) {
    const bool valid =
        mado::check(flash != nullptr) && mado::check(strlen_P(flash) < kLedgerPathSize);
    out[0] = '\0';
    if (valid) {
      static_cast<void>(strncpy_P(&out[0], flash, sizeof(out) - 1U));
    }
    return valid;
  }

  static bool read_line(mado::FileSource<FileT>& source, char (&line)[kLedgerLineSize]) {
    uint8_t length = 0U;
    int16_t symbol = source.next();
    for (uint8_t i = 0U; (symbol >= 0) && (symbol != '\n') && (i < kLedgerLineSize); ++i) {
      line[length] = static_cast<char>(symbol);
      length = static_cast<uint8_t>(length + 1U);
      symbol = source.next();
    }
    const bool ended = mado::check(symbol == '\n');
    const bool fits = mado::check(length < kLedgerLineSize);
    line[fits ? length : 0U] = '\0';
    return ended && fits;
  }

  static bool parse_record(const char* const line, PostRecord& out) {
    const bool shaped = mado::check(strnlen(line, kLedgerLineSize) > (kFingerprintDigits + 3U)) &&
                        (line[kFingerprintDigits] == ' ') && (line[kFingerprintDigits + 2U] == ' ');
    uint32_t fingerprint = 0U;
    bool hex = shaped;
    for (uint8_t i = 0U; hex && (i < kFingerprintDigits); ++i) {
      const char digit = line[i];
      const bool decimal = (digit >= '0') && (digit <= '9');
      hex = decimal || ((digit >= 'a') && (digit <= 'f'));
      const uint8_t nibble = static_cast<uint8_t>(decimal ? (digit - '0') : ((digit - 'a') + 10));
      fingerprint = (fingerprint << 4U) | nibble;
    }
    const uint8_t item_id = static_cast<uint8_t>(line[kFingerprintDigits + 1U] - '0');
    if (!(hex && mado::check(is_item_id(item_id)))) {
      return false;
    }
    out = PostRecord{};
    static_cast<void>(strncpy(&out.key[0], &line[kFingerprintDigits + 3U], kKeySize - 1U));
    out.fingerprint = fingerprint;
    out.item_id = item_id;
    return true;
  }

  static bool write_record(mado::FileSink<FileT>& sink, const PostRecord& record) {
    const bool keyed = mado::check(record.key[0] != '\0');
    const bool numbered = mado::check(is_item_id(record.item_id));
    char head[kFingerprintDigits + 4U] = {};
    for (uint8_t i = 0U; i < kFingerprintDigits; ++i) {
      const uint8_t shift = static_cast<uint8_t>(28U - (4U * i));
      const uint8_t nibble = static_cast<uint8_t>((record.fingerprint >> shift) & 0x0FU);
      head[i] = static_cast<char>((nibble < 10U) ? ('0' + nibble) : ('a' + (nibble - 10U)));
    }
    head[kFingerprintDigits] = ' ';
    head[kFingerprintDigits + 1U] = static_cast<char>('0' + (numbered ? record.item_id : 0U));
    head[kFingerprintDigits + 2U] = ' ';
    return keyed && numbered && (sink.print(&head[0]) > 0U) && (sink.print(&record.key[0]) > 0U) &&
           (sink.write(static_cast<uint8_t>('\n')) == 1U);
  }

  bool save() {
    char path[kLedgerPathSize] = {};
    char temporary[kLedgerPathSize] = {};
    const bool named =
        copy_path(&kLedgerPath[0], path) && copy_path(&kLedgerTemporary[0], temporary);
    const bool clear =
        mado::check(named) && ((!card_->exists(&temporary[0])) || card_->remove(&temporary[0]));
    if (!clear) {
      return false;
    }
    FileT file = card_->open(&temporary[0], kWriteMode);
    if (!mado::check(static_cast<bool>(file))) {
      return false;
    }
    mado::FileSink<FileT> sink(file);
    bool written = true;
    for (uint8_t i = 0U; written && (i < kLedgerSize); ++i) {
      written = (records_[i].key[0] == '\0') || write_record(sink, records_[i]);
    }
    file.close();
    return written && mado::replace_file<kReadMode, kWriteMode>(*card_, &temporary[0], &path[0]);
  }

  PostRecord records_[kLedgerSize] = {};
  uint8_t next_ = 0U;
  SdT* card_ = nullptr;
};

}

#endif
