// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_READ_BYTE_H
#define MADO_READ_BYTE_H

#include <stdint.h>

#include "ReadStatus.h"
#include "check.h"
#include "wait_for_data.h"

namespace mado {

template <typename LimitsT, typename ClientT>
ReadStatus read_byte(ClientT& client, uint8_t& out, const uint32_t request_started) {
  int32_t available = 0;
  const ReadStatus waited = wait_for_data<LimitsT>(client, available, request_started);
  if (waited != ReadStatus::kByte) {
    return waited;
  }
  const bool has_data = check(available > 0);
  const int32_t value = has_data ? client.read() : -1;
  const bool in_range = check(value >= 0) && check(value <= 0xFF);
  if (!in_range) {
    return ReadStatus::kClosed;
  }
  out = static_cast<uint8_t>(value);
  return ReadStatus::kByte;
}

}

#endif
