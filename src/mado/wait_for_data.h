// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_WAIT_FOR_DATA_H
#define MADO_WAIT_FOR_DATA_H

#include <Arduino.h>
#include <stdint.h>

#include "ReadStatus.h"

namespace mado {

template <typename LimitsT, typename ClientT>
ReadStatus wait_for_data(ClientT& client, int32_t& available, const uint32_t request_started) {
  static_assert(LimitsT::kMaxPollsPerRead > 0U, "polling needs at least one attempt");
  static_assert(LimitsT::kReadTimeoutMs > 0U, "a read timeout must be positive");
  static_assert(LimitsT::kRequestTimeoutMs >= LimitsT::kReadTimeoutMs,
                "a request deadline must allow one full read");
  available = 0;
  const uint32_t started = millis();
  for (uint32_t poll = 0U; poll < LimitsT::kMaxPollsPerRead; ++poll) {
    const uint32_t now = millis();
    const bool request_expired = (now - request_started) >= LimitsT::kRequestTimeoutMs;
    if (request_expired) {
      return ReadStatus::kTimeout;
    }
    const int32_t count = client.available();
    if (count > 0) {
      available = count;
      return ReadStatus::kByte;
    }
    const bool connected = client.connected() != 0U;
    if (!connected) {
      return ReadStatus::kClosed;
    }
    const bool read_expired = (now - started) >= LimitsT::kReadTimeoutMs;
    if (read_expired) {
      return ReadStatus::kTimeout;
    }
  }
  return ReadStatus::kTimeout;
}

}

#endif
