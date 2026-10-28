// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_AUDIT_REQUEST_JOURNAL_SERVICE_H
#define REST_API_AUDIT_REQUEST_JOURNAL_SERVICE_H

#include <Arduino.h>
#include <mado/FileLength.h>
#include <mado/LogEntry.h>
#include <mado/RequestLine.h>
#include <mado/ResponseSummary.h>
#include <mado/SdLog.h>
#include <mado/check.h>
#include <mado/file_length.h>
#include <stdint.h>

#include "../domain/response_time.policy.h"

namespace audit {

constexpr uint32_t kLogMaxBytes = 65536UL;
const char kLogPath[] PROGMEM = "/LOG.TXT";
const char kLogOldPath[] PROGMEM = "/LOG.OLD";

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class RequestJournal {
 public:
  using Log = mado::SdLog<SdT, kReadMode, kWriteMode>;
  using FileT = typename Log::FileT;

  void attach(SdT* const card, const uint32_t session) {
    card_ = card;
    session_ = mado::check(session > 0U) ? session : 0U;
    requests_ = 0U;
    log_.attach(card);
  }

  bool request(const mado::RequestLine& line, const mado::ResponseSummary& response,
               const uint32_t now_ms) {
    requests_ = requests_ + 1U;
    const mado::RequestLog entry = {
        requests_,
        line.method,
        (line.path == nullptr) ? "" : line.path,
        response.status,
        response_ms(response.started_ms, now_ms),
    };
    const bool counted = mado::check(response.status >= 100U) && mado::check(entry.url != nullptr);
    return counted && log_.request(stamp(now_ms), entry);
  }

  bool event(const uint8_t level, const __FlashStringHelper* const message, const uint32_t now_ms) {
    const bool valid = mado::check(level >= mado::kLogInfo) && mado::check(message != nullptr);
    return valid && log_.event(stamp(now_ms), level, message);
  }

  bool clear() {
    const bool mounted = mado::check(card_ != nullptr);
    return mounted && mado::check(log_.clear());
  }

  mado::FileLength open(FileT& file) {
    char path[mado::kLogPathSize] = {};
    const bool named = mado::check(card_ != nullptr) && log_.path(path);
    if (!(named && card_->exists(&path[0]))) {
      return mado::FileLength{false, 0U};
    }
    file = card_->open(&path[0], kReadMode);
    return mado::check(static_cast<bool>(file)) ? mado::file_length(file)
                                                : mado::FileLength{false, 0U};
  }

 private:
  mado::LogStamp stamp(const uint32_t now_ms) const {
    const bool started = mado::check(session_ > 0U);
    const mado::LogStamp value = {started ? session_ : 0U, now_ms, F("madoguchi")};
    return value;
  }

  SdT* card_ = nullptr;
  uint32_t session_ = 0U;
  uint32_t requests_ = 0U;
  Log log_{&kLogPath[0], &kLogOldPath[0], kLogMaxBytes};
};

}

#endif
