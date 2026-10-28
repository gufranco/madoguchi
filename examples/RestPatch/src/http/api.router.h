// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_PATCH_API_ROUTER_H
#define REST_PATCH_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../items/adapters/items.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename RepositoryT, typename CardT, uint16_t kReadMode>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_patch::Limits>;
  using Items = items::ItemsController<Exchange, RepositoryT>;

  static void attach(RepositoryT& repository, CardT* const card) {
    Wiring& wired = wiring();
    wired.repository = &repository;
    wired.card = card;
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_patch::kCardFile);
    const Wiring& wired = wiring();
    if (!known) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    if (!mado::check(wired.repository != nullptr)) {
      return mado::send_problem(
          exchange, mado::Problem{mado::http::kServiceUnavailable, F("no item store is attached")});
    }
    return (route == rest_patch::kUpdateItem)
               ? Items::update(exchange, *wired.repository, route)
               : rest_patch::send_card<kReadMode>(exchange, wired.card, route);
  }

 private:
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
