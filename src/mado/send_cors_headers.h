// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_CORS_HEADERS_H
#define MADO_SEND_CORS_HEADERS_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "FlashText.h"
#include "check.h"

namespace mado {

constexpr uint8_t kMaxCorsOrigins = 8U;
constexpr uint32_t kCorsMaxAgeSeconds = 600UL;

namespace detail {

inline bool origin_listed(const char* const origin, const char* const* const origins) {
  const bool valid = check(origin != nullptr) && check(origins != nullptr);
  bool listed = false;
  for (uint8_t i = 0U; valid && (!listed) && (i < kMaxCorsOrigins); ++i) {
    const char* const candidate = origins[i];
    if (candidate == nullptr) {
      break;
    }
    listed = strcmp(candidate, origin) == 0;
  }
  return listed;
}

}

template <typename ExchangeT>
bool send_cors_headers(ExchangeT& exchange, const char* const* const origins) {
  const bool ready = check(origins != nullptr) && check(exchange.status_sent());
  if (!ready) {
    return false;
  }
  const bool varied = exchange.send_header(F("Vary"), FlashText(F("Origin")));
  const char* const origin = exchange.header(F("Origin"));
  const bool allowed = (origin != nullptr) && detail::origin_listed(origin, origins);
  if (!allowed) {
    return varied;
  }
  const bool echoed = varied && exchange.send_header(F("Access-Control-Allow-Origin"), origin);
  const bool methods =
      echoed && exchange.send_header(F("Access-Control-Allow-Methods"),
                                     FlashText(F("GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS")));
  const bool headers = methods && exchange.send_header(F("Access-Control-Allow-Headers"),
                                                       FlashText(F("Content-Type, Authorization")));
  return headers && exchange.send_header(F("Access-Control-Max-Age"), kCorsMaxAgeSeconds);
}

}

#endif
