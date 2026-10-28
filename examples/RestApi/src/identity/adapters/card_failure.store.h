// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_IDENTITY_CARD_FAILURE_STORE_H
#define REST_API_IDENTITY_CARD_FAILURE_STORE_H

#include <Arduino.h>
#include <mado/FileSink.h>
#include <mado/FileSource.h>
#include <mado/GuardEntry.h>
#include <mado/LoginGuard.h>
#include <mado/check.h>
#include <mado/reference_to.h>
#include <mado/replace_file.h>
#include <stdint.h>
#include <string.h>

#include "../application/token_authority.port.h"

namespace identity {

constexpr uint8_t kFailurePathSize = 13U;
constexpr uint8_t kAddressDigits = 8U;
constexpr uint8_t kFailureLineSize = kAddressDigits + 4U;
const char kFailurePath[] PROGMEM = "/LOGIN.TXT";
const char kFailureTemporaryPath[] PROGMEM = "/LOGIN.TMP";

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class CardFailureLog {
 public:
  using FileT = decltype(mado::reference_to<SdT>().open("", kReadMode));

  explicit CardFailureLog(SdT* const card) : card_(card) {
  }

  bool load(mado::GuardEntry (&entries)[kGuardSlots]) {
    static_cast<void>(memset(&entries[0], 0, sizeof(entries)));
    char path[kFailurePathSize] = {};
    const bool named = copy_path(&kFailurePath[0], path);
    if (!(named && mado::check(card_ != nullptr) && card_->exists(&path[0]))) {
      return false;
    }
    FileT file = card_->open(&path[0], kReadMode);
    mado::FileSource<FileT> source(file);
    bool read = static_cast<bool>(file);
    for (uint8_t i = 0U; read && (i < kGuardSlots); ++i) {
      char line[kFailureLineSize] = {};
      read = read_line(source, line) && parse_entry(&line[0], entries[i]);
    }
    file.close();
    return mado::check(read);
  }

  bool save(const mado::GuardEntry (&entries)[kGuardSlots]) {
    char path[kFailurePathSize] = {};
    char temporary[kFailurePathSize] = {};
    const bool named =
        copy_path(&kFailurePath[0], path) && copy_path(&kFailureTemporaryPath[0], temporary);
    const bool clear = named && mado::check(card_ != nullptr) &&
                       ((!card_->exists(&temporary[0])) || card_->remove(&temporary[0]));
    if (!clear) {
      return false;
    }
    FileT file = card_->open(&temporary[0], kWriteMode);
    if (!mado::check(static_cast<bool>(file))) {
      return false;
    }
    mado::FileSink<FileT> sink(file);
    bool written = true;
    for (uint8_t i = 0U; written && (i < kGuardSlots); ++i) {
      written = write_entry(sink, entries[i]);
    }
    file.close();
    return written && mado::replace_file<kReadMode, kWriteMode>(*card_, &temporary[0], &path[0]);
  }

 private:
  static bool copy_path(const char* const flash, char (&out)[kFailurePathSize]) {
    const bool valid =
        mado::check(flash != nullptr) && mado::check(strlen_P(flash) < kFailurePathSize);
    out[0] = '\0';
    if (valid) {
      static_cast<void>(strncpy_P(&out[0], flash, sizeof(out) - 1U));
    }
    return valid;
  }

  static bool read_line(mado::FileSource<FileT>& source, char (&line)[kFailureLineSize]) {
    uint8_t length = 0U;
    int16_t symbol = source.next();
    for (uint8_t i = 0U; (symbol >= 0) && (symbol != '\n') && (i < kFailureLineSize); ++i) {
      line[length] = static_cast<char>(symbol);
      length = static_cast<uint8_t>(length + 1U);
      symbol = source.next();
    }
    const bool ended = mado::check(symbol == '\n');
    const bool fits = mado::check(length < kFailureLineSize);
    line[fits ? length : 0U] = '\0';
    return ended && fits;
  }

  static bool parse_entry(const char* const line, mado::GuardEntry& out) {
    const bool shaped = mado::check(strnlen(line, kFailureLineSize) == (kAddressDigits + 2U)) &&
                        (line[kAddressDigits] == ' ');
    uint32_t address = 0U;
    bool hex = shaped;
    for (uint8_t i = 0U; hex && (i < kAddressDigits); ++i) {
      const char digit = line[i];
      const bool decimal = (digit >= '0') && (digit <= '9');
      hex = decimal || ((digit >= 'a') && (digit <= 'f'));
      const uint8_t nibble = static_cast<uint8_t>(decimal ? (digit - '0') : ((digit - 'a') + 10));
      address = (address << 4U) | nibble;
    }
    const uint8_t failures = static_cast<uint8_t>(line[kAddressDigits + 1U] - '0');
    const bool counted = mado::check(failures <= mado::LoginGuard::kMaxFailures);
    out = (hex && counted) ? mado::GuardEntry{address, failures} : mado::GuardEntry{0U, 0U};
    return hex && counted;
  }

  static bool write_entry(mado::FileSink<FileT>& sink, const mado::GuardEntry& entry) {
    const bool counted = mado::check(entry.failures <= mado::LoginGuard::kMaxFailures);
    const bool named = (entry.failures == 0U) || mado::check(entry.address != 0U);
    char line[kFailureLineSize] = {};
    for (uint8_t i = 0U; i < kAddressDigits; ++i) {
      const uint8_t shift = static_cast<uint8_t>(28U - (4U * i));
      const uint8_t nibble = static_cast<uint8_t>((entry.address >> shift) & 0x0FU);
      line[i] = static_cast<char>((nibble < 10U) ? ('0' + nibble) : ('a' + (nibble - 10U)));
    }
    line[kAddressDigits] = ' ';
    line[kAddressDigits + 1U] = static_cast<char>('0' + (counted ? entry.failures : 0U));
    line[kAddressDigits + 2U] = '\n';
    return counted && named && (sink.print(&line[0]) == (kAddressDigits + 3U));
  }

  SdT* card_;
};

}

#endif
