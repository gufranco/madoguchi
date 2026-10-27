// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LOAD_MAC_H
#define MADO_LOAD_MAC_H

#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint8_t kMacSize = 6U;
constexpr uint8_t kMacMarker = 0xA6U;
constexpr uint8_t kMacStoredBytes = 7U;

namespace detail {

inline uint32_t next_random(const uint32_t state) {
  const bool seeded = check(state != 0U);
  uint32_t value = seeded ? state : 0x9E3779B9UL;
  value ^= (value << 13U);
  value ^= (value >> 17U);
  value ^= (value << 5U);
  return check(value != 0U) ? value : 0x9E3779B9UL;
}

}

template <typename StoreT>
bool load_mac(StoreT& store, const uint16_t address, uint8_t (&mac)[kMacSize],
              const uint32_t seed) {
  const bool fits = check((static_cast<uint32_t>(address) + kMacStoredBytes) <= store.length());
  if (!fits) {
    return false;
  }
  const int base = static_cast<int>(address);
  const bool stored = store.read(base) == kMacMarker;
  uint32_t state = seed;
  for (uint8_t i = 0U; i < kMacSize; ++i) {
    state = detail::next_random(state);
    const uint8_t fresh = static_cast<uint8_t>(state >> 24U);
    mac[i] = stored ? store.read(base + 1 + i) : fresh;
  }
  mac[0] = static_cast<uint8_t>((mac[0] & 0xFCU) | 0x02U);
  for (uint8_t i = 0U; (!stored) && (i < kMacSize); ++i) {
    store.update(base + 1 + i, mac[i]);
  }
  store.update(base, kMacMarker);
  return check((mac[0] & 0x03U) == 0x02U) && stored;
}

}

#endif
