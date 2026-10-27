// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ISSUE_TOKEN_H
#define MADO_ISSUE_TOKEN_H

#include <Arduino.h>
#include <stdint.h>

#include "TokenClaims.h"
#include "TokenWriter.h"
#include "check.h"
#include "kMaxTokenClaimValue.h"
#include "key_provisioned.h"
#include "token_header_segment.h"
#include "valid_token_subject.h"

namespace mado {

template <typename ExchangeT>
bool issue_token(ExchangeT& exchange, const uint8_t* const flash_key, const TokenClaims& claims) {
  const bool valid = check(valid_token_subject(&claims.subject[0])) &&
                     check(claims.session <= kMaxTokenClaimValue) &&
                     check(claims.ends_at <= kMaxTokenClaimValue);
  if ((!valid) || (!key_provisioned(flash_key))) {
    return false;
  }
  TokenWriter<ExchangeT> writer(exchange, flash_key);
  writer.signed_flash(token_header_segment());
  writer.signed_char('.');
  writer.encoded_flash(PSTR("{\"sub\":\""));
  writer.encoded_ram(&claims.subject[0]);
  writer.encoded_flash(PSTR("\",\"ses\":"));
  writer.encoded_number(claims.session);
  writer.encoded_flash(PSTR(",\"end\":"));
  writer.encoded_number(claims.ends_at);
  writer.encoded_flash(PSTR("}"));
  writer.end_segment();
  return writer.sign();
}

}

#endif
