// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_COUNT_FILE_H
#define MADO_COUNT_FILE_H

#include <stdint.h>

#include "FileSink.h"
#include "check.h"
#include "replace_file.h"

namespace mado {

constexpr uint8_t kNoCount = 0xFFU;
constexpr uint8_t kMaxCountDigits = 3U;

template <uint8_t kReadMode, typename SdT>
uint8_t read_count_file(SdT& card, const char* const path) {
  const bool valid = check(path != nullptr);
  if ((!valid) || (!card.exists(path))) {
    return kNoCount;
  }
  auto file = card.open(path, kReadMode);
  uint8_t digits[kMaxCountDigits + 1U] = {};
  const int32_t got = static_cast<bool>(file) ? file.read(&digits[0], sizeof(digits)) : -1;
  file.close();
  const bool bounded = check(got <= static_cast<int32_t>(sizeof(digits)));
  uint32_t value = 0U;
  uint8_t used = 0U;
  for (; bounded && (got > 0) && (used < static_cast<uint8_t>(got)) && (used < kMaxCountDigits);
       ++used) {
    const uint8_t digit = digits[used];
    if ((digit < '0') || (digit > '9')) {
      break;
    }
    value = (value * 10U) + (digit - '0');
  }
  const bool parsed = check(used <= kMaxCountDigits) && (used > 0U) && (value < kNoCount);
  return parsed ? static_cast<uint8_t>(value) : kNoCount;
}

template <uint8_t kReadMode, uint8_t kWriteMode, typename SdT>
bool write_count_file(SdT& card, const char* const path, const char* const temporary,
                      const uint8_t value) {
  const bool valid = check(path != nullptr) && check(temporary != nullptr);
  if ((!valid) || (value == kNoCount)) {
    return false;
  }
  const bool cleared = (!card.exists(temporary)) || card.remove(temporary);
  auto file = card.open(temporary, kWriteMode);
  if ((!cleared) || (!static_cast<bool>(file))) {
    return false;
  }
  FileSink<decltype(file)> sink(file);
  const bool written = (sink.print(static_cast<uint32_t>(value)) > 0U) &&
                       (sink.write(static_cast<uint8_t>('\n')) == 1U);
  file.close();
  return written && replace_file<kReadMode, kWriteMode>(card, temporary, path);
}

}

#endif
