// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_GET_API_ROUTER_H
#define REST_GET_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../identity/adapters/auth.controller.h"
#include "../identity/adapters/token.gate.h"
#include "../items/adapters/items.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename AccessT, typename RepositoryT, typename CardT,
          uint16_t kReadMode>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_get::Limits>;
  using Auth = identity::AuthController<Exchange, AccessT>;
  using Gate = identity::TokenGate<Exchange, AccessT>;
  using Items = items::ItemsController<Exchange, RepositoryT>;

  static void attach(RepositoryT& repository, CardT* const card) {
    Wiring& wired = wiring();
    wired.repository = &repository;
    wired.card = card;
  }

  static void attach_access(AccessT& access, mado::UptimeClock& clock) {
    Gatekeeper& keeper = gatekeeper();
    keeper.access = &access;
    keeper.clock = &clock;
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    if (!admitted(exchange, route)) {
      return true;
    }
    const bool known = mado::check(route <= rest_get::kCardFile);
    const Wiring& wired = wiring();
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    if (!mado::check(wired.repository != nullptr)) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kServiceUnavailable, F("no item store is attached")});
    }
    switch (route) {
      case rest_get::kListItems: return Items::list(exchange, *wired.repository);
      case rest_get::kGetItem: return Items::get(exchange, *wired.repository, route);
      default: return rest_get::send_card<kReadMode>(exchange, wired.card, route);
    }
  }

 private:
  struct Gatekeeper {
    AccessT* access;
    mado::UptimeClock* clock;
  };

  static Gatekeeper& gatekeeper() {
    static Gatekeeper instance = {nullptr, nullptr};
    return instance;
  }

  static bool admitted(Exchange& exchange, const uint8_t route) {
    const Gatekeeper& keeper = gatekeeper();
    if (!(mado::check(keeper.access != nullptr) && mado::check(keeper.clock != nullptr))) {
      const bool sent = mado::send_problem(exchange, mado::Problem{
                                                         mado::http::kServiceUnavailable,
                                                         F("the sign-in service is not attached"),
                                                     });
      static_cast<void>(sent);
      return false;
    }
    keeper.clock->advance(millis());
    const uint32_t now = keeper.clock->seconds();
    if (route == rest_get::kLogin) {
      const bool answered = Auth::login(exchange, *keeper.access, now);
      static_cast<void>(answered);
      return false;
    }
    return (!rest_get::route_needs_token(route)) || Gate::admitted(exchange, *keeper.access, now);
  }

  struct Wiring {
    RepositoryT* repository;
    CardT* card;
  };

  static Wiring& wiring() {
    static Wiring instance = {nullptr, nullptr};
    return instance;
  }
};

}

#endif
