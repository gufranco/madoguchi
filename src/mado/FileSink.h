// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_SINK_H
#define MADO_FILE_SINK_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "DecimalText.h"
#include "check.h"
#include "flash_address.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kFileSinkBlock = 32U;

template <typename FileT>
class FileSink {
 public:
  explicit FileSink(FileT& file) : file_(&file) {
  }

  size_t write(const uint8_t value) {
    const size_t written = write(&value, 1U);
    return check(written <= 1U) ? written : 0U;
  }

  size_t write(const uint8_t* const data, const size_t length) {
    const bool valid = check(file_ != nullptr) && (check(data != nullptr) || check(length == 0U));
    if ((!valid) || (!ok_) || (length == 0U)) {
      return 0U;
    }
    const size_t written = file_->write(data, length);
    ok_ = written == length;
    return ok_ ? length : 0U;
  }

  size_t print(const char* const text) {
    const bool valid = check(text != nullptr);
    const size_t length = valid ? strlen(text) : 0U;
    const bool bounded = check(length < kMaxStringScan);
    return bounded ? write_text(text, length) : 0U;
  }

  size_t print(const __FlashStringHelper* const text) {
    const char* const flash = flash_address(text);
    const bool valid = check(flash != nullptr);
    const size_t length = valid ? strlen_P(flash) : 0U;
    uint8_t block[kFileSinkBlock] = {};
    size_t written = 0U;
    for (size_t done = 0U; ok_ && check(done <= length) && (done < length);) {
      const size_t count = ((length - done) < kFileSinkBlock) ? (length - done) : kFileSinkBlock;
      static_cast<void>(memcpy_P(&block[0], flash + done, count));
      written += write(&block[0], count);
      done += count;
    }
    return ok_ ? written : 0U;
  }

  size_t print(const uint32_t value) {
    const DecimalText digits(value);
    const bool sized = check(digits.length() > 0U) && check(digits.length() <= 10U);
    const size_t length = sized ? digits.length() : 0U;
    return write_text(digits.begin(), length);
  }

  size_t print_signed(const int32_t value) {
    const bool negative = value < 0;
    const size_t sign = negative ? write(static_cast<uint8_t>('-')) : 0U;
    const uint32_t magnitude =
        negative ? (static_cast<uint32_t>(-(value + 1)) + 1UL) : static_cast<uint32_t>(value);
    const bool signed_ok = check((!negative) || (sign == 1U)) && check(magnitude <= 0x80000000UL);
    return signed_ok ? (sign + print(magnitude)) : 0U;
  }

  bool ok() const {
    const bool wired = check(file_ != nullptr);
    return wired && ok_;
  }

 private:
  size_t write_text(const char* const text, const size_t length) {
    const bool valid = check(text != nullptr) && check(length < kMaxStringScan);
    uint8_t block[kFileSinkBlock] = {};
    size_t written = 0U;
    for (size_t done = 0U; valid && ok_ && (done < length);) {
      const size_t count = ((length - done) < kFileSinkBlock) ? (length - done) : kFileSinkBlock;
      static_cast<void>(memcpy(&block[0], text + done, count));
      written += write(&block[0], count);
      done += count;
    }
    return ok_ ? written : 0U;
  }

  FileT* file_;
  bool ok_ = true;
};

}

#endif
