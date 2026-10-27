// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_BEGIN_NETWORK_H
#define MADO_BEGIN_NETWORK_H

#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint32_t kDhcpResponseTimeoutMs = 4000UL;
constexpr uint32_t kMinDhcpTimeoutMs = 1000UL;

template <typename EthernetT, typename AddressT>
bool begin_network(EthernetT& ethernet, uint8_t* const mac, const AddressT& fallback,
                   const uint32_t timeout_ms) {
  const bool valid = check(mac != nullptr) && check(timeout_ms >= kMinDhcpTimeoutMs);
  if (!valid) {
    return false;
  }
  const int32_t leased = ethernet.begin(mac, timeout_ms, kDhcpResponseTimeoutMs);
  if (leased == 1) {
    return true;
  }
  ethernet.begin(mac, fallback);
  return false;
}

}

#endif
