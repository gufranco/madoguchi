// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_GET_IDENTITY_SESSION_VALUE_H
#define REST_GET_IDENTITY_SESSION_VALUE_H

#include <mado/check.h>
#include <stdint.h>

namespace identity {

constexpr uint32_t kLastClaimValue = 0x7FFFFFFFU;
constexpr uint32_t kTokenLifetime = 900U;

inline uint32_t next_session(const uint32_t stored) {
  const bool counted = mado::check(stored != 0U) && mado::check(stored < kLastClaimValue);
  return counted ? (stored + 1U) : 1U;
}

inline bool token_ends_at(const uint32_t now, uint32_t& ends_at) {
  const bool fits = mado::check(now <= (kLastClaimValue - kTokenLifetime));
  ends_at = fits ? (now + kTokenLifetime) : 0U;
  return fits && mado::check(ends_at > now);
}

}

#endif
