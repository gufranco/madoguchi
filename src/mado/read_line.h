// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_READ_LINE_H
#define MADO_READ_LINE_H

#include <stdint.h>

#include "LineStatus.h"
#include "ReadStatus.h"
#include "check.h"
#include "read_byte.h"

namespace mado {

template <typename LimitsT, typename ClientT>
LineStatus read_line(ClientT& client, const uint32_t request_started, char* const buffer,
                     const uint16_t size) {
  const bool valid = check(buffer != nullptr) && check(size > 1U);
  if (!valid) {
    return LineStatus::kTooLong;
  }
  buffer[0] = '\0';
  uint16_t length = 0U;
  for (uint32_t i = 0U; i <= size; ++i) {
    uint8_t value = 0U;
    const ReadStatus status = read_byte<LimitsT>(client, value, request_started);
    if (status != ReadStatus::kByte) {
      buffer[length] = '\0';
      return (status == ReadStatus::kTimeout) ? LineStatus::kTimeout : LineStatus::kClosed;
    }
    if (value == static_cast<uint8_t>('\n')) {
      buffer[length] = '\0';
      return LineStatus::kOk;
    }
    const bool is_return = value == static_cast<uint8_t>('\r');
    const bool full = length >= (size - 1U);
    if ((!is_return) && full) {
      buffer[length] = '\0';
      return LineStatus::kTooLong;
    }
    if (!is_return) {
      buffer[length] = static_cast<char>(value);
      ++length;
    }
  }
  buffer[length] = '\0';
  return LineStatus::kTooLong;
}

}

#endif
