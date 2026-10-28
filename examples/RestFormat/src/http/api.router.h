// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_API_ROUTER_H
#define REST_FORMAT_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../card/adapters/format.controller.h"
#include "../identity/adapters/auth.controller.h"
#include "../identity/adapters/token.gate.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename AccessT, typename ServiceT>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_format::Limits>;
  using Auth = identity::AuthController<Exchange, AccessT>;
  using Gate = identity::TokenGate<Exchange, AccessT>;
  using Format = card::FormatController<Exchange, ServiceT>;

  static void attach(ServiceT& service) {
    wiring() = &service;
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
    const bool known = mado::check(route <= rest_format::kStartFormat);
    ServiceT* const service = wiring();
    if (!(known && mado::check(service != nullptr))) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    switch (route) {
      case rest_format::kGetCard: return Format::get_card(exchange, *service);
      case rest_format::kGetFormat: return Format::get_format(exchange, *service);
      default: return Format::start(exchange, *service);
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
    if (route == rest_format::kLogin) {
      const bool answered = Auth::login(exchange, *keeper.access, now);
      static_cast<void>(answered);
      return false;
    }
    return (!rest_format::route_needs_token(route)) ||
           Gate::admitted(exchange, *keeper.access, now);
  }

  static ServiceT*& wiring() {
    static ServiceT* instance = nullptr;
    return instance;
  }
};

}

#endif
