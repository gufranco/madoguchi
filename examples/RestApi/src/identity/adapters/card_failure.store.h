// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_IDENTITY_CARD_FAILURE_STORE_H
#define REST_API_IDENTITY_CARD_FAILURE_STORE_H

#include <Arduino.h>
#include <mado/LoginGuard.h>
#include <mado/check.h>
#include <mado/count_file.h>
#include <stdint.h>
#include <string.h>

namespace identity {

constexpr uint8_t kFailurePathSize = 13U;
const char kFailurePath[] PROGMEM = "/LOGIN.TXT";
const char kFailureTemporaryPath[] PROGMEM = "/STORE.TMP";

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class CardFailureLog {
 public:
  explicit CardFailureLog(SdT* const card) : card_(card) {
  }

  uint8_t load() {
    char path[kFailurePathSize] = {};
    const bool named = copy_path(&kFailurePath[0], path);
    const bool mounted = mado::check(card_ != nullptr);
    return (named && mounted) ? mado::read_count_file<kReadMode>(*card_, &path[0]) : mado::kNoCount;
  }

  bool save(const uint8_t failures) {
    if (!mado::check(failures <= mado::LoginGuard::kMaxFailures)) {
      return false;
    }
    char path[kFailurePathSize] = {};
    char temporary[kFailurePathSize] = {};
    const bool named =
        copy_path(&kFailurePath[0], path) && copy_path(&kFailureTemporaryPath[0], temporary);
    const bool mounted = mado::check(card_ != nullptr);
    return named && mounted &&
           mado::write_count_file<kReadMode, kWriteMode>(*card_, &path[0], &temporary[0], failures);
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

  SdT* card_;
};

}

#endif
