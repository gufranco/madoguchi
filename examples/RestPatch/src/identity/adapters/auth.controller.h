// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_IDENTITY_AUTH_CONTROLLER_H
#define REST_PATCH_IDENTITY_AUTH_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/LoginGuard.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_bearer_challenge.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/access.service.h"
#include "../domain/credentials.value.h"
#include "../domain/session.value.h"

namespace identity {

constexpr uint32_t kMaxLoginBody = 256U;

template <typename ExchangeT, typename AccessT>
class AuthController {
 public:
  static bool login(ExchangeT& exchange, AccessT& access, const uint32_t now) {
    const uint32_t address = exchange.client_address();
    const Admission admission = access.admit(address, now);
    if (!mado::check(admission == Admission::kOpen)) {
      return turn_away(exchange, access, admission, address, now);
    }
    rest_patch::Credentials body = {};
    uint8_t present = 0U;
    const mado::Problem problem =
        rest_patch::read_credentials_body(exchange, body, present, kMaxLoginBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    char token[kTokenTextSize] = {};
    const SignInStatus status = access.sign_in(credentials_of(body), address, now, token);
    if (!mado::check(status == SignInStatus::kSignedIn)) {
      return (status == SignInStatus::kRefused)
                 ? mado::send_bearer_challenge(exchange, mado::TokenStatus::kMissing)
                 : mado::send_problem(exchange, mado::Problem{
                                                    mado::http::kInternalServerError,
                                                    F("the token could not be signed"),
                                                });
    }
    return send_token(exchange, token);
  }

 private:
  static_assert(sizeof(rest_patch::Credentials::username) == kUserNameSize, "the schema agrees");
  static_assert(sizeof(rest_patch::Credentials::password) == kPasswordSize, "the schema agrees");
  static_assert(sizeof(rest_patch::Token::access_token) == kTokenTextSize, "the schema agrees");

  static bool turn_away(ExchangeT& exchange, const AccessT& access, const Admission admission,
                        const uint32_t address, const uint32_t now) {
    const bool refused = mado::check(admission != Admission::kOpen);
    if (refused && mado::check(admission == Admission::kUnprovisioned)) {
      return mado::send_problem(exchange, mado::Problem{
                                              mado::http::kServiceUnavailable,
                                              F("the token key is not provisioned"),
                                          });
    }
    mado::Reply<ExchangeT> reply(exchange);
    return reply.status(mado::http::kTooManyRequests)
        .header(F("Retry-After"), access.retry_after(address, now))
        .length(0U)
        .end();
  }

  static Credentials credentials_of(const rest_patch::Credentials& body) {
    const bool named = mado::check(body.username[kUserNameSize - 1U] == '\0');
    const bool secret = mado::check(body.password[kPasswordSize - 1U] == '\0');
    Credentials credentials = {};
    static_cast<void>(
        memcpy(&credentials.username[0], &body.username[0], sizeof(credentials.username)));
    static_cast<void>(
        memcpy(&credentials.password[0], &body.password[0], sizeof(credentials.password)));
    return (named && secret) ? credentials : Credentials{};
  }

  static bool send_token(ExchangeT& exchange, const char (&text)[kTokenTextSize]) {
    rest_patch::Token token = {};
    const bool signed_text =
        mado::check(text[0] != '\0') && mado::check(text[kTokenTextSize - 1U] == '\0');
    static_cast<void>(memcpy(&token.access_token[0], &text[0], sizeof(token.access_token)));
    static_cast<void>(
        strncpy_P(&token.token_type[0], PSTR("Bearer"), sizeof(token.token_type) - 1U));
    token.expires_in = static_cast<int32_t>(kTokenLifetime);
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = signed_text && reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_patch::write_token(exchange, token);
  }
};

}

#endif
