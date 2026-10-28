// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_AUTH_GREETINGS_GREETINGS_CONTROLLER_H
#define REST_AUTH_GREETINGS_GREETINGS_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_bearer_challenge.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../domain/greeting.value.h"

namespace greetings {

template <typename ExchangeT, typename AccessT>
class GreetingsController {
 public:
  static bool greet(ExchangeT& exchange, const AccessT& access, const uint32_t now) {
    mado::TokenClaims claims = {};
    const mado::TokenStatus status =
        access.authorize(exchange.header(F("Authorization")), now, claims);
    if (!mado::check(status == mado::TokenStatus::kValid)) {
      return mado::send_bearer_challenge(exchange, status);
    }
    Greeting greeting = {};
    if (!mado::check(greetings::greet(&claims.subject[0], greeting))) {
      return mado::send_problem(exchange, mado::Problem{
                                              mado::http::kInternalServerError,
                                              F("the greeting does not fit"),
                                          });
    }
    rest_auth::Greeting dto = {};
    static_cast<void>(memcpy(&dto.message[0], &greeting.message[0], sizeof(dto.message)));
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = reply.status(mado::http::kOk).json().no_store().end();
    return head && rest_auth::write_greeting(exchange, dto);
  }

 private:
  static_assert(sizeof(rest_auth::Greeting::message) == kMessageSize, "the schema agrees");
};

}

#endif
