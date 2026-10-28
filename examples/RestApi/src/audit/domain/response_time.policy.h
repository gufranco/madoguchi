// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_AUDIT_RESPONSE_TIME_POLICY_H
#define REST_API_AUDIT_RESPONSE_TIME_POLICY_H

#include <mado/check.h>
#include <stdint.h>

namespace audit {

constexpr uint32_t kLongestResponseMs = 0x80000000UL;

inline uint32_t response_ms(const uint32_t started_ms, const uint32_t now_ms) {
  const uint32_t elapsed = now_ms - started_ms;
  const bool sane = mado::check(elapsed < kLongestResponseMs);
  return sane ? elapsed : 0U;
}

}

#endif
