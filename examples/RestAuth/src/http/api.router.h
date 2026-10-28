// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_AUTH_API_ROUTER_H
#define REST_AUTH_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../greetings/adapters/greetings.controller.h"
#include "../identity/adapters/auth.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename AccessT, typename CardT, uint16_t kReadMode>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_auth::Limits>;
  using Auth = identity::AuthController<Exchange, AccessT>;
  using Greetings = greetings::GreetingsController<Exchange, AccessT>;

  static void attach(AccessT& access, mado::UptimeClock& clock, CardT* const card) {
    Wiring& wired = wiring();
    wired.access = &access;
    wired.clock = &clock;
    wired.card = card;
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_auth::kCardFile);
    const Wiring& wired = wiring();
    if (!known) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});
    }
    if (!(mado::check(wired.access != nullptr) && mado::check(wired.clock != nullptr))) {
      return mado::send_problem(exchange, mado::Problem{
                                              mado::http::kServiceUnavailable,
                                              F("the sign-in service is not attached"),
                                          });
    }
    wired.clock->advance(millis());
    const uint32_t now = wired.clock->seconds();
    switch (route) {
      case rest_auth::kLogin: return Auth::login(exchange, *wired.access, now);
      case rest_auth::kGetGreeting: return Greetings::greet(exchange, *wired.access, now);
      default: return rest_auth::send_card<kReadMode>(exchange, wired.card, route);
    }
  }

 private:
  struct Wiring {
    AccessT* access;
    mado::UptimeClock* clock;
    CardT* card;
  };

  static Wiring& wiring() {
    static Wiring instance = {nullptr, nullptr, nullptr};
    return instance;
  }
};

}

#endif
