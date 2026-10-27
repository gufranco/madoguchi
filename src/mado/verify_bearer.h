// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_VERIFY_BEARER_H
#define MADO_VERIFY_BEARER_H

#include <Arduino.h>
#include <stdint.h>

#include "Base64UrlDecoder.h"
#include "HmacSha256.h"
#include "KeyMemory.h"
#include "TokenClaims.h"
#include "TokenStatus.h"
#include "check.h"
#include "constant_time_equal.h"
#include "kMaxTokenClaimValue.h"
#include "kMaxTokenSubject.h"
#include "kSha256Size.h"
#include "kTokenKeySize.h"
#include "key_provisioned.h"
#include "parse_token_claims.h"
#include "to_lower_ascii.h"
#include "token_header_segment.h"

namespace mado {

constexpr uint8_t kMaxTokenLength = 200U;
constexpr uint8_t kMaxTokenPayload = 64U;

namespace detail {

constexpr uint8_t kBearerSchemeLength = 6U;

struct TokenLayout {
  const char* text;
  uint8_t first_dot;
  uint8_t second_dot;
  uint8_t length;
  bool valid;
};

inline const char* bearer_token(const char* const authorization) {
  const bool present = check(authorization != nullptr);
  if (!present) {
    return nullptr;
  }
  const char* const scheme = PSTR("bearer");
  for (uint8_t i = 0U; i < kBearerSchemeLength; ++i) {
    const char expected = static_cast<char>(pgm_read_byte(scheme + i));
    if (to_lower_ascii(authorization[i]) != expected) {
      return nullptr;
    }
  }
  const bool spaced = authorization[kBearerSchemeLength] == ' ';
  return spaced ? (authorization + kBearerSchemeLength + 1U) : nullptr;
}

inline TokenLayout token_layout(const char* const token) {
  TokenLayout layout = {token, 0U, 0U, 0U, false};
  const bool present = check(token != nullptr);
  uint8_t dots = 0U;
  uint8_t length = 0U;
  for (; present && (length < kMaxTokenLength) && (token[length] != '\0'); ++length) {
    if (token[length] != '.') {
      continue;
    }
    dots = static_cast<uint8_t>(dots + 1U);
    layout.first_dot = (dots == 1U) ? length : layout.first_dot;
    layout.second_dot = (dots == 2U) ? length : layout.second_dot;
  }
  layout.length = length;
  const bool bounded = check(length <= kMaxTokenLength);
  const bool ended = bounded && present && (token[length] == '\0');
  layout.valid = ended && (dots == 2U) && (layout.first_dot > 0U) &&
                 (layout.second_dot > (layout.first_dot + 1U)) &&
                 (layout.length > (layout.second_dot + 1U));
  return layout;
}

inline bool header_matches(const TokenLayout& layout) {
  const bool valid = check(layout.valid) && check(layout.text != nullptr);
  if ((!valid) || (layout.first_dot != kTokenHeaderLength)) {
    return false;
  }
  return strncmp_P(layout.text, token_header_segment(), kTokenHeaderLength) == 0;
}

inline bool decode_segment(const char* const text, const uint8_t length, uint8_t* const out,
                           const uint8_t capacity, uint8_t& decoded) {
  const bool valid = check(text != nullptr) && check(out != nullptr);
  decoded = 0U;
  Base64UrlDecoder decoder;
  uint8_t chunk[Base64UrlDecoder::kMaxOutput] = {};
  bool intact = valid;
  for (uint8_t i = 0U; intact && (i <= length); ++i) {
    const int8_t count =
        (i < length) ? decoder.push(text[i], &chunk[0]) : decoder.finish(&chunk[0]);
    intact = (count >= 0) &&
             ((static_cast<uint16_t>(decoded) + static_cast<uint8_t>(count)) <= capacity);
    for (int8_t j = 0; intact && (j < count); ++j) {
      out[decoded] = chunk[j];
      decoded = static_cast<uint8_t>(decoded + 1U);
    }
  }
  return intact;
}

inline bool signature_matches(const TokenLayout& layout, const uint8_t* const flash_key) {
  const bool valid = check(layout.valid) && check(flash_key != nullptr);
  HmacSha256 mac;
  const bool keyed = valid && mac.begin(flash_key, kTokenKeySize, KeyMemory::kFlash);
  bool absorbed = keyed;
  for (uint8_t i = 0U; absorbed && (i < layout.second_dot); ++i) {
    absorbed = mac.update(static_cast<uint8_t>(layout.text[i]));
  }
  uint8_t expected[kSha256Size] = {};
  uint8_t presented[kSha256Size] = {};
  uint8_t decoded = 0U;
  const uint8_t start = static_cast<uint8_t>(layout.second_dot + 1U);
  const bool computed = absorbed && mac.finish(&expected[0]);
  const bool read =
      computed && decode_segment(layout.text + start, static_cast<uint8_t>(layout.length - start),
                                 &presented[0], kSha256Size, decoded);
  const bool equal = constant_time_equal(&expected[0], &presented[0], kSha256Size);
  return read && (decoded == kSha256Size) && equal;
}

inline TokenStatus check_payload(const TokenLayout& layout, TokenClaims& claims) {
  const bool valid = check(layout.valid) && check(layout.text != nullptr);
  uint8_t payload[kMaxTokenPayload] = {};
  uint8_t decoded = 0U;
  const uint8_t start = static_cast<uint8_t>(layout.first_dot + 1U);
  const uint8_t length = static_cast<uint8_t>(layout.second_dot - start);
  const bool read =
      valid && decode_segment(layout.text + start, length, &payload[0], sizeof(payload), decoded);
  const bool parsed = read && parse_token_claims(&payload[0], decoded, claims);
  return parsed ? TokenStatus::kValid : TokenStatus::kMalformed;
}

}

inline TokenStatus verify_bearer(const char* const authorization, const uint8_t* const flash_key,
                                 const uint32_t session, const uint32_t now, TokenClaims& claims) {
  claims.subject[0] = '\0';
  claims.session = 0U;
  claims.ends_at = 0U;
  const bool keyed = check(flash_key != nullptr) && key_provisioned(flash_key);
  if (!keyed) {
    return TokenStatus::kUnprovisioned;
  }
  const bool signable = check(session <= kMaxTokenClaimValue) && check(now <= kMaxTokenClaimValue);
  if (!signable) {
    return TokenStatus::kExpired;
  }
  if (authorization == nullptr) {
    return TokenStatus::kMissing;
  }
  const detail::TokenLayout layout = detail::token_layout(detail::bearer_token(authorization));
  if ((!layout.valid) || (!detail::header_matches(layout))) {
    return TokenStatus::kMalformed;
  }
  if (!detail::signature_matches(layout, flash_key)) {
    return TokenStatus::kBadSignature;
  }
  const TokenStatus payload = detail::check_payload(layout, claims);
  if (payload != TokenStatus::kValid) {
    return payload;
  }
  if (claims.session != session) {
    return TokenStatus::kStaleSession;
  }
  return (now < claims.ends_at) ? TokenStatus::kValid : TokenStatus::kExpired;
}

}

#endif
