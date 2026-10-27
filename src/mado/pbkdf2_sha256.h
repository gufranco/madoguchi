// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PBKDF2_SHA256_H
#define MADO_PBKDF2_SHA256_H

#include <stdint.h>

#include "HmacSha256.h"
#include "KeyMemory.h"
#include "Pbkdf2Params.h"
#include "check.h"
#include "kSha256Size.h"

namespace mado {

constexpr uint32_t kMaxPbkdf2Iterations = 1000000U;

namespace detail {

inline bool first_pbkdf2_block(HmacSha256& mac, const uint8_t* const salt,
                               const uint8_t salt_length, uint8_t* const block) {
  const bool valid = check(salt != nullptr) && check(block != nullptr);
  if (!valid) {
    return false;
  }
  const uint8_t index[] = {0U, 0U, 0U, 1U};
  const bool salted = mac.update(salt, salt_length);
  const bool indexed = mac.update(&index[0], sizeof(index));
  return salted && indexed && mac.finish(block);
}

inline bool next_pbkdf2_block(HmacSha256& mac, uint8_t* const block, uint8_t* const derived) {
  const bool valid = check(block != nullptr) && check(derived != nullptr);
  if (!valid) {
    return false;
  }
  const bool restarted = mac.restart();
  const bool absorbed = mac.update(block, kSha256Size);
  const bool done = restarted && absorbed && mac.finish(block);
  for (uint8_t i = 0U; i < kSha256Size; ++i) {
    derived[i] = static_cast<uint8_t>(derived[i] ^ block[i]);
  }
  return done;
}

}

inline bool pbkdf2_sha256(const Pbkdf2Params& params, uint8_t* const derived) {
  const bool valid = check(params.password != nullptr) && check(params.salt != nullptr) &&
                     check(derived != nullptr) && check(params.iterations > 0U) &&
                     check(params.iterations <= kMaxPbkdf2Iterations);
  if (!valid) {
    return false;
  }
  HmacSha256 mac;
  uint8_t block[kSha256Size] = {};
  const bool keyed = mac.begin(params.password, params.password_length, KeyMemory::kRam);
  bool intact =
      keyed && detail::first_pbkdf2_block(mac, params.salt, params.salt_length, &block[0]);
  for (uint8_t i = 0U; i < kSha256Size; ++i) {
    derived[i] = block[i];
  }
  for (uint32_t round = 1U; intact && (round < params.iterations); ++round) {
    intact = detail::next_pbkdf2_block(mac, &block[0], derived);
  }
  return intact;
}

}

#endif
