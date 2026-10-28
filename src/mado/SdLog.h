// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SD_LOG_H
#define MADO_SD_LOG_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "FileSink.h"
#include "LogEntry.h"
#include "check.h"
#include "reference_to.h"
#include "replace_file.h"
#include "write_log_line.h"

namespace mado {

constexpr uint8_t kLogPathSize = 13U;

template <typename SdT, uint16_t kReadMode, uint16_t kWriteMode>
class SdLog {
  static_assert(kLogPathSize >= 13U, "an 8.3 path with its slash fits");

 public:
  using FileT = decltype(reference_to<SdT>().open("", kWriteMode));

  SdLog(const char* const flash_path, const char* const flash_old_path, const uint32_t max_bytes)
      : path_(flash_path), old_path_(flash_old_path), max_bytes_(max_bytes) {
    const bool named = check(flash_path != nullptr) && check(flash_old_path != nullptr);
    path_ = named ? flash_path : nullptr;
  }

  void attach(SdT* const card) {
    const bool named = check(path_ != nullptr) && check(old_path_ != nullptr);
    sd_ = named ? card : nullptr;
  }

  bool request(const LogStamp& stamp, const RequestLog& entry) {
    const bool valid = check(entry.url != nullptr);
    FileT file = valid ? open_for_append() : FileT();
    if (!static_cast<bool>(file)) {
      return false;
    }
    FileSink<FileT> sink(file);
    const bool written = write_request_log(sink, stamp, entry);
    file.close();
    return written && check(sink.ok() || (!written));
  }

  bool event(const LogStamp& stamp, const uint8_t level, const __FlashStringHelper* const message) {
    const bool valid = check(level >= kLogInfo) && check(message != nullptr);
    FileT file = valid ? open_for_append() : FileT();
    if (!static_cast<bool>(file)) {
      return false;
    }
    FileSink<FileT> sink(file);
    const bool written = write_event_log(sink, stamp, level, message);
    file.close();
    return written && check(sink.ok() || (!written));
  }

  bool clear() {
    char current[kLogPathSize] = {};
    char old_path[kLogPathSize] = {};
    const bool named = copy_path(path_, current) && copy_path(old_path_, old_path);
    if ((!named) || (sd_ == nullptr)) {
      return false;
    }
    const bool distinct = check(strcmp(&current[0], &old_path[0]) != 0);
    const bool gone = (!sd_->exists(&current[0])) || sd_->remove(&current[0]);
    const bool rotated = (!sd_->exists(&old_path[0])) || sd_->remove(&old_path[0]);
    return distinct && gone && rotated && check(!sd_->exists(&current[0]));
  }

  bool path(char (&out)[kLogPathSize]) const {
    return copy_path(path_, out);
  }

 private:
  static bool copy_path(const char* const flash, char (&out)[kLogPathSize]) {
    const bool valid = check(flash != nullptr) && check(strlen_P(flash) < kLogPathSize);
    out[0] = '\0';
    if (valid) {
      static_cast<void>(strncpy_P(&out[0], flash, sizeof(out) - 1U));
    }
    return valid;
  }

  FileT open_for_append() {
    char current[kLogPathSize] = {};
    char old_path[kLogPathSize] = {};
    const bool named =
        (sd_ != nullptr) && copy_path(path_, current) && copy_path(old_path_, old_path);
    if (!named) {
      return FileT();
    }
    FileT file = sd_->open(&current[0], kWriteMode);
    const bool limited = check(max_bytes_ > 0U);
    const bool full = limited && static_cast<bool>(file) && (file.size() >= max_bytes_);
    if (!full) {
      return file;
    }
    file.close();
    const bool rotated = replace_file<kReadMode, kWriteMode>(*sd_, &current[0], &old_path[0]);
    static_cast<void>(rotated);
    return sd_->open(&current[0], kWriteMode);
  }

  const char* path_;
  const char* old_path_;
  uint32_t max_bytes_;
  SdT* sd_ = nullptr;
};

}

#endif
