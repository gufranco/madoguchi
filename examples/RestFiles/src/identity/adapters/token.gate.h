// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_IDENTITY_TOKEN_GATE_H
#define REST_FILES_IDENTITY_TOKEN_GATE_H

#include <Arduino.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <mado/send_bearer_challenge.h>
#include <stdint.h>

namespace identity {

template <typename ExchangeT, typename AccessT>
struct TokenGate {
  static bool admitted(ExchangeT& exchange, const AccessT& access, const uint32_t now) {
    mado::TokenClaims claims = {};
    const mado::TokenStatus status =
        access.authorize(exchange.header(F("Authorization")), now, claims);
    if (mado::check(status == mado::TokenStatus::kValid)) {
      return mado::check(claims.subject[0] != '\0');
    }
    const bool sent = mado::send_bearer_challenge(exchange, status);
    static_cast<void>(sent);
    return false;
  }
};

}

#endif
