// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PUT_IDENTITY_FLASH_SECRETS_AUTHORITY_H
#define REST_PUT_IDENTITY_FLASH_SECRETS_AUTHORITY_H

#include <Arduino.h>
#include <mado/MemorySink.h>
#include <mado/Pbkdf2Params.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <mado/constant_time_equal.h>
#include <mado/issue_token.h>
#include <mado/kMaxTokenClaimValue.h>
#include <mado/kSha256Size.h>
#include <mado/key_provisioned.h>
#include <mado/pbkdf2_sha256.h>
#include <mado/verify_bearer.h>
#include <stdint.h>
#include <string.h>

#include "../application/token_authority.port.h"
#include "../domain/credentials.value.h"
#include "../domain/session.value.h"

namespace identity {

constexpr uint8_t kSaltSize = 16U;

template <typename SecretsT>
class FlashSecretsAuthority {
 public:
  static_assert(mado::kMaxTokenClaimValue == kLastClaimValue, "claims share one upper bound");

  static bool provisioned() {
    return mado::check(mado::key_provisioned(SecretsT::token_key()));
  }

  static bool matches(const Credentials& credentials) {
    const bool same_user = user_matches(credentials);
    const bool same_password = password_matches(credentials);
    return mado::check(same_user) && mado::check(same_password);
  }

  static bool sign(const mado::TokenClaims& claims, char (&token)[kTokenTextSize]) {
    const bool bounded = mado::check(claims.ends_at <= kLastClaimValue) &&
                         mado::check(claims.session <= kLastClaimValue);
    mado::MemorySink sink(&token[0], sizeof(token));
    return bounded && mado::issue_token(sink, SecretsT::token_key(), claims);
  }

  static mado::TokenStatus verify(const char* const header, const uint32_t session,
                                  const uint32_t now, mado::TokenClaims& claims) {
    const bool bounded =
        mado::check(session <= kLastClaimValue) && mado::check(now <= kLastClaimValue);
    return bounded ? mado::verify_bearer(header, SecretsT::token_key(), session, now, claims)
                   : mado::TokenStatus::kExpired;
  }

 private:
  static uint8_t copy_bytes(const char* const text, uint8_t* const out, const uint8_t size) {
    const bool valid = mado::check(text != nullptr) && mado::check(out != nullptr);
    uint8_t length = 0U;
    for (; valid && (length < size) && (text[length] != '\0'); ++length) {
      out[length] = static_cast<uint8_t>(text[length]);
    }
    const bool terminated = mado::check(length < size);
    return terminated ? length : 0U;
  }

  static bool user_matches(const Credentials& credentials) {
    char stored[kUserNameSize] = {};
    static_cast<void>(strncpy_P(&stored[0], SecretsT::user_name(), sizeof(stored) - 1U));
    uint8_t expected[kUserNameSize] = {};
    uint8_t given[kUserNameSize] = {};
    const uint8_t stored_length = copy_bytes(&stored[0], &expected[0], sizeof(expected));
    const uint8_t given_length = copy_bytes(&credentials.username[0], &given[0], sizeof(given));
    const bool sized = mado::check(stored_length > 0U) && mado::check(given_length > 0U);
    return mado::constant_time_equal(&expected[0], &given[0], sizeof(expected)) && sized;
  }

  static bool password_matches(const Credentials& credentials) {
    uint8_t password[kPasswordSize] = {};
    uint8_t salt[kSaltSize] = {};
    uint8_t expected[mado::kSha256Size] = {};
    uint8_t derived[mado::kSha256Size] = {};
    const uint8_t length = copy_bytes(&credentials.password[0], &password[0], sizeof(password));
    memcpy_P(&salt[0], SecretsT::password_salt(), sizeof(salt));
    memcpy_P(&expected[0], SecretsT::password_hash(), sizeof(expected));
    const mado::Pbkdf2Params params = {
        &password[0], length, &salt[0], sizeof(salt), SecretsT::password_iterations(),
    };
    const bool derived_ok = mado::check(length > 0U) && mado::pbkdf2_sha256(params, &derived[0]);
    return mado::constant_time_equal(&expected[0], &derived[0], sizeof(derived)) && derived_ok;
  }
};

}

#endif
