// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_BEARER_CHALLENGE_H
#define MADO_SEND_BEARER_CHALLENGE_H

#include <Arduino.h>

#include "FlashText.h"
#include "TokenStatus.h"
#include "check.h"
#include "http_status.h"

namespace mado {

template <typename ExchangeT>
bool send_bearer_challenge(ExchangeT& exchange, const TokenStatus status) {
  const bool refused = check(status != TokenStatus::kValid) && check(!exchange.status_sent());
  if (!refused) {
    return false;
  }
  if (status == TokenStatus::kUnprovisioned) {
    return exchange.send_empty(http::kServiceUnavailable);
  }
  const bool sent = exchange.send_status(http::kUnauthorized);
  const FlashText challenge = (status == TokenStatus::kMissing)
                                  ? FlashText(F("Bearer realm=\"api\""))
                                  : FlashText(F("Bearer realm=\"api\", error=\"invalid_token\""));
  const bool header = sent && exchange.send_header(F("WWW-Authenticate"), challenge);
  const bool length = header && exchange.send_content_length(0U);
  return length && exchange.end_headers();
}

}

#endif
