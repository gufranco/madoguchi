// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef AUTH_APP_H
#define AUTH_APP_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/LoginGuard.h>
#include <mado/MemorySink.h>
#include <mado/Pbkdf2Params.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/constant_time_equal.h>
#include <mado/count_file.h>
#include <mado/http_status.h>
#include <mado/issue_token.h>
#include <mado/kMaxTokenClaimValue.h>
#include <mado/kSha256Size.h>
#include <mado/key_provisioned.h>
#include <mado/pbkdf2_sha256.h>
#include <mado/send_bearer_challenge.h>
#include <mado/send_problem.h>
#include <mado/verify_bearer.h>
#include <stdint.h>
#include <string.h>

#include "generated_api.h"

constexpr uint32_t kAuthTokenLifetime = 900U;
constexpr uint32_t kAuthMaxRequestBody = 256U;
constexpr uint8_t kAuthSaltSize = 16U;

constexpr uint8_t kAuthPathSize = 13U;
const char kAuthLoginPath[] PROGMEM = "/LOGIN.TXT";
const char kAuthTemporaryPath[] PROGMEM = "/LOGIN.TMP";

template <typename SdT>
struct AuthState {
  SdT* sd = nullptr;
  mado::UptimeClock clock;
  mado::LoginGuard guard;
  uint32_t session = 0U;
};

template <typename ClientT, typename SecretsT, typename SdT, uint16_t kReadMode,
          uint16_t kWriteMode>
struct AuthApp {
  using Exchange = mado::Exchange<ClientT, rest_auth::Limits>;

  static AuthState<SdT>& state() {
    static AuthState<SdT> instance = {};
    return instance;
  }

  static void begin(const uint32_t session, SdT& sd, const uint8_t chip_select) {
    const bool signable = mado::check(session <= mado::kMaxTokenClaimValue);
    state() = AuthState<SdT>{};
    state().session = signable ? session : 0U;
    state().sd = sd.begin(chip_select) ? &sd : nullptr;
    char path[kAuthPathSize] = {};
    static_cast<void>(strncpy_P(&path[0], &kAuthLoginPath[0], sizeof(path) - 1U));
    const uint8_t stored =
        (state().sd == nullptr) ? mado::kNoCount : mado::read_count_file<kReadMode>(sd, &path[0]);
    state().guard.restore(stored);
  }

  static void tick(const uint32_t now_ms) {
    const bool started = mado::check(state().session <= mado::kMaxTokenClaimValue);
    if (started) {
      state().clock.advance(now_ms);
    }
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    tick(millis());
    const bool known = mado::check(route <= rest_auth::kCardFile);
    if (!known) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});
    }
    switch (route) {
      case rest_auth::kLogin: return login(exchange);
      case rest_auth::kGetGreeting: return greet(exchange);
      default: return rest_auth::send_card<kReadMode>(exchange, state().sd, route);
    }
  }

 private:
  static bool greet(Exchange& exchange) {
    mado::TokenClaims claims = {};
    const mado::TokenStatus status =
        mado::verify_bearer(exchange.header(F("Authorization")), SecretsT::token_key(),
                            state().session, state().clock.seconds(), claims);
    if (status != mado::TokenStatus::kValid) {
      return mado::send_bearer_challenge(exchange, status);
    }
    rest_auth::Greeting greeting = {};
    strncpy_P(&greeting.message[0], PSTR("Hello, "), sizeof(greeting.message) - 1U);
    const size_t used = strlen(&greeting.message[0]);
    const bool fits = mado::check((used + strlen(&claims.subject[0])) < sizeof(greeting.message));
    if (fits) {
      strncpy(&greeting.message[used], &claims.subject[0], sizeof(greeting.message) - used - 1U);
    }
    const bool head = fits && send_json_head(exchange, mado::http::kOk);
    return head && rest_auth::write_greeting(exchange, greeting);
  }

  static bool send_json_head(Exchange& exchange, const uint16_t code) {
    const bool success = mado::check((code == mado::http::kOk) || (code == mado::http::kCreated));
    mado::Reply<Exchange> reply(exchange);
    return success && reply.status(code).json().no_store().end();
  }

  static bool login(Exchange& exchange) {
    const uint32_t now = state().clock.seconds();
    if (!mado::key_provisioned(SecretsT::token_key())) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kServiceUnavailable,
                                                        F("the token key is not provisioned")});
    }
    if (state().guard.locked(now)) {
      return send_locked(exchange, now);
    }
    rest_auth::Credentials credentials = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_auth::read_credentials_body(exchange, credentials, present, kAuthMaxRequestBody);
    if (problem.status != mado::http::kNoResponse) {
      return mado::send_problem(exchange, problem);
    }
    if (!credentials_match(credentials)) {
      state().guard.record_failure(now);
      remember_failures();
      return mado::send_bearer_challenge(exchange, mado::TokenStatus::kMissing);
    }
    state().guard.record_success();
    remember_failures();
    return send_token(exchange, credentials, now);
  }

  static void remember_failures() {
    char path[kAuthPathSize] = {};
    char temporary[kAuthPathSize] = {};
    static_cast<void>(strncpy_P(&path[0], &kAuthLoginPath[0], sizeof(path) - 1U));
    static_cast<void>(strncpy_P(&temporary[0], &kAuthTemporaryPath[0], sizeof(temporary) - 1U));
    const bool wired = mado::check(path[0] == '/') && (state().sd != nullptr);
    const bool saved = wired && mado::write_count_file<kReadMode, kWriteMode>(
                                    *state().sd, &path[0], &temporary[0], state().guard.failures());
    static_cast<void>(saved);
  }

  static bool send_locked(Exchange& exchange, const uint32_t now) {
    const uint32_t wait = state().guard.retry_after(now);
    const bool waiting =
        mado::check(wait > 0U) && mado::check(wait <= mado::LoginGuard::kLockSeconds);
    mado::Reply<Exchange> reply(exchange);
    return reply.status(mado::http::kTooManyRequests)
        .header(F("Retry-After"), waiting ? wait : 1U)
        .length(0U)
        .end();
  }

  static uint8_t copy_bytes(const char* const text, uint8_t* const out, const uint8_t size) {
    const bool valid = mado::check(text != nullptr) && mado::check(out != nullptr);
    uint8_t length = 0U;
    for (; valid && (length < size) && (text[length] != '\0'); ++length) {
      out[length] = static_cast<uint8_t>(text[length]);
    }
    const bool terminated = mado::check(length < size);
    return terminated ? length : 0U;
  }

  static bool user_matches(const rest_auth::Credentials& credentials) {
    char stored[sizeof(credentials.username)] = {};
    strncpy_P(&stored[0], SecretsT::user_name(), sizeof(stored) - 1U);
    uint8_t expected[sizeof(stored)] = {};
    uint8_t given[sizeof(stored)] = {};
    const uint8_t stored_length = copy_bytes(&stored[0], &expected[0], sizeof(expected));
    const uint8_t given_length = copy_bytes(&credentials.username[0], &given[0], sizeof(given));
    const bool sized = mado::check(stored_length > 0U) && (given_length > 0U);
    return mado::constant_time_equal(&expected[0], &given[0], sizeof(expected)) && sized;
  }

  static bool password_matches(const rest_auth::Credentials& credentials) {
    uint8_t password[sizeof(credentials.password)] = {};
    uint8_t salt[kAuthSaltSize] = {};
    uint8_t expected[mado::kSha256Size] = {};
    uint8_t derived[mado::kSha256Size] = {};
    const uint8_t length = copy_bytes(&credentials.password[0], &password[0], sizeof(password));
    memcpy_P(&salt[0], SecretsT::password_salt(), sizeof(salt));
    memcpy_P(&expected[0], SecretsT::password_hash(), sizeof(expected));
    const mado::Pbkdf2Params params = {&password[0], length, &salt[0], sizeof(salt),
                                       SecretsT::password_iterations()};
    const bool derived_ok = mado::check(length > 0U) && mado::pbkdf2_sha256(params, &derived[0]);
    return mado::constant_time_equal(&expected[0], &derived[0], sizeof(derived)) && derived_ok;
  }

  static bool credentials_match(const rest_auth::Credentials& credentials) {
    const bool same_user = user_matches(credentials);
    const bool same_password = password_matches(credentials);
    return same_user && same_password;
  }

  static bool send_token(Exchange& exchange, const rest_auth::Credentials& credentials,
                         const uint32_t now) {
    mado::TokenClaims claims = {};
    strncpy(&claims.subject[0], &credentials.username[0], sizeof(claims.subject) - 1U);
    claims.session = state().session;
    claims.ends_at = now + kAuthTokenLifetime;
    rest_auth::Token token = {};
    mado::MemorySink sink(&token.access_token[0], sizeof(token.access_token));
    const bool signable = mado::check(claims.ends_at <= mado::kMaxTokenClaimValue);
    const bool issued = signable && mado::issue_token(sink, SecretsT::token_key(), claims);
    strncpy_P(&token.token_type[0], PSTR("Bearer"), sizeof(token.token_type) - 1U);
    token.expires_in = static_cast<int32_t>(kAuthTokenLifetime);
    const bool head = issued && send_json_head(exchange, mado::http::kOk);
    return head && rest_auth::write_token(exchange, token);
  }
};

#endif
