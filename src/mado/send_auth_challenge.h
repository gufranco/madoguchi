// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_AUTH_CHALLENGE_H
#define MADO_SEND_AUTH_CHALLENGE_H

#include <Arduino.h>
#include <stddef.h>

#include "check.h"
#include "http_status.h"

namespace mado {

template <typename ExchangeT>
bool send_auth_challenge(ExchangeT& exchange, const __FlashStringHelper* const realm) {
  const bool valid = check(realm != nullptr) && check(!exchange.status_sent());
  if (!valid) {
    return false;
  }
  const bool status = exchange.send_status(http::kUnauthorized);
  const size_t opened = exchange.print(F("WWW-Authenticate: Basic realm=\""));
  const size_t named = exchange.print(realm);
  const size_t closed = exchange.print(F("\", charset=\"UTF-8\"\r\n"));
  const bool sized = exchange.send_content_length(0UL);
  const bool ended = exchange.end_headers();
  const bool printed = (opened > 0U) && (named > 0U) && (closed > 0U);
  return status && printed && sized && ended;
}

}

#endif
