// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_TOKEN_CLAIMS_H
#define MADO_PARSE_TOKEN_CLAIMS_H

#include <Arduino.h>
#include <stdint.h>

#include "TokenClaims.h"
#include "check.h"
#include "kMaxTokenClaimValue.h"
#include "kMaxTokenSubject.h"

namespace mado {

namespace detail {

constexpr uint8_t kMaxClaimDigits = 10U;

struct ClaimsCursor {
  const uint8_t* data;
  uint8_t length;
  uint8_t position;
};

inline bool expect_literal(ClaimsCursor& cursor, const char* const literal) {
  const bool valid = check(cursor.data != nullptr) && check(literal != nullptr);
  bool same = valid;
  for (uint8_t i = 0U; same && (i < cursor.length); ++i) {
    const uint8_t wanted = pgm_read_byte(literal + i);
    if (wanted == 0U) {
      return true;
    }
    same = (cursor.position < cursor.length) && (cursor.data[cursor.position] == wanted);
    cursor.position = static_cast<uint8_t>(cursor.position + 1U);
  }
  return false;
}

inline bool subject_char(const uint8_t value) {
  const bool letter = ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z'));
  const bool digit = (value >= '0') && (value <= '9');
  return letter || digit || (value == '.') || (value == '_') || (value == '-');
}

inline bool read_subject(ClaimsCursor& cursor, char* const subject) {
  const bool valid = check(cursor.data != nullptr) && check(subject != nullptr);
  uint8_t used = 0U;
  for (; valid && (used < kMaxTokenSubject) && (cursor.position < cursor.length); ++used) {
    const uint8_t value = cursor.data[cursor.position];
    if (!subject_char(value)) {
      break;
    }
    subject[used] = static_cast<char>(value);
    cursor.position = static_cast<uint8_t>(cursor.position + 1U);
  }
  subject[valid ? used : 0U] = '\0';
  return valid && (used > 0U);
}

inline bool read_claim_number(ClaimsCursor& cursor, uint32_t& value) {
  const bool valid = check(cursor.data != nullptr) && check(cursor.position <= cursor.length);
  value = 0U;
  uint8_t digits = 0U;
  for (; valid && (digits <= kMaxClaimDigits) && (cursor.position < cursor.length); ++digits) {
    const uint8_t symbol = cursor.data[cursor.position];
    if ((symbol < '0') || (symbol > '9')) {
      break;
    }
    const uint32_t digit = static_cast<uint32_t>(symbol - '0');
    if (((digits > 0U) && (value == 0U)) || (value > ((kMaxTokenClaimValue - digit) / 10U))) {
      return false;
    }
    value = (value * 10U) + digit;
    cursor.position = static_cast<uint8_t>(cursor.position + 1U);
  }
  return valid && (digits > 0U) && (digits <= kMaxClaimDigits);
}

}

inline bool parse_token_claims(const uint8_t* const payload, const uint8_t length,
                               TokenClaims& claims) {
  const bool valid = check(payload != nullptr) && check(length > 0U);
  detail::ClaimsCursor cursor = {payload, length, 0U};
  bool intact = valid && detail::expect_literal(cursor, PSTR("{\"sub\":\""));
  intact = intact && detail::read_subject(cursor, &claims.subject[0]);
  intact = intact && detail::expect_literal(cursor, PSTR("\",\"ses\":"));
  intact = intact && detail::read_claim_number(cursor, claims.session);
  intact = intact && detail::expect_literal(cursor, PSTR(",\"end\":"));
  intact = intact && detail::read_claim_number(cursor, claims.ends_at);
  intact = intact && detail::expect_literal(cursor, PSTR("}"));
  return intact && (cursor.position == cursor.length);
}

}

#endif
