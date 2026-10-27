// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_METHOD_NOT_ALLOWED_H
#define MADO_SEND_METHOD_NOT_ALLOWED_H

#include <Arduino.h>
#include <stdint.h>

#include "Method.h"
#include "Route.h"
#include "check.h"
#include "http_status.h"
#include "match_route.h"
#include "route_at.h"

namespace mado {

namespace detail {

constexpr uint8_t kAllowListSize = 48U;
constexpr uint8_t kMethodNamesLength = 38U;

inline uint16_t method_bit(const Method method) {
  const bool listed = check(method != Method::kUnknown) && check(method != Method::kAny);
  return listed ? static_cast<uint16_t>(1U << static_cast<uint8_t>(method)) : 0U;
}

template <uint8_t MaxRoutes>
uint16_t allowed_methods(const Route* const routes, const char* const path) {
  const bool valid = check(routes != nullptr) && check(path != nullptr);
  uint16_t mask = 0U;
  for (uint8_t i = 0U; valid && (i < MaxRoutes); ++i) {
    const Route route = route_at(routes, i);
    if (route.path == nullptr) {
      break;
    }
    const bool listed = (route.method != Method::kAny) && (route.method != Method::kUnknown);
    if (listed && match_route(route.path, path)) {
      mask = static_cast<uint16_t>(mask | method_bit(route.method));
    }
  }
  const bool has_get = (mask & method_bit(Method::kGet)) != 0U;
  return has_get ? static_cast<uint16_t>(mask | method_bit(Method::kHead)) : mask;
}

inline bool format_allow(const uint16_t mask, char* const list, const uint8_t size) {
  const bool valid = check(list != nullptr) && check(size == kAllowListSize);
  const char* const names = PSTR("GET HEAD POST PUT DELETE PATCH OPTIONS");
  uint8_t method = static_cast<uint8_t>(Method::kGet);
  uint8_t used = 0U;
  bool word_start = true;
  for (uint8_t i = 0U; valid && (i < kMethodNamesLength) && ((used + 3U) < size); ++i) {
    const char symbol = static_cast<char>(pgm_read_byte(names + i));
    const bool separator = symbol == ' ';
    method = static_cast<uint8_t>(method + (separator ? 1U : 0U));
    const bool allowed = (!separator) && ((mask & (1U << method)) != 0U);
    if (allowed && word_start && (used > 0U)) {
      list[used] = ',';
      list[used + 1U] = ' ';
      used = static_cast<uint8_t>(used + 2U);
    }
    if (allowed) {
      list[used] = symbol;
      used = static_cast<uint8_t>(used + 1U);
    }
    word_start = separator;
  }
  const bool fits = check(used < size);
  list[fits ? used : 0U] = '\0';
  return valid && fits;
}

}

template <uint8_t MaxRoutes, typename ExchangeT>
bool send_method_not_allowed(ExchangeT& exchange, const Route* const routes,
                             const char* const path) {
  const bool valid = check(routes != nullptr) && check(path != nullptr);
  const uint16_t mask = valid ? detail::allowed_methods<MaxRoutes>(routes, path) : 0U;
  char list[detail::kAllowListSize] = {};
  const bool listed = detail::format_allow(mask, &list[0], sizeof(list));
  const bool status = exchange.send_status(http::kMethodNotAllowed);
  const bool allow = listed && exchange.send_header(F("Allow"), &list[0]);
  const bool length = exchange.send_content_length(0U);
  const bool ended = exchange.end_headers();
  return status && allow && length && ended;
}

}

#endif
