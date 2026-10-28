// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_API_ROUTER_H
#define REST_FORMAT_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <stdint.h>

#include "../card/adapters/format.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename ServiceT>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_format::Limits>;
  using Format = card::FormatController<Exchange, ServiceT>;

  static void attach(ServiceT& service) {
    wiring() = &service;
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
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
  static ServiceT*& wiring() {
    static ServiceT* instance = nullptr;
    return instance;
  }
};

}

#endif
