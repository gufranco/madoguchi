// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_API_ROUTER_H
#define REST_FILES_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../files/adapters/files.controller.h"
#include "../identity/adapters/auth.controller.h"
#include "../identity/adapters/token.gate.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename AccessT, typename LibraryT, typename CardT, typename FileT,
          uint16_t kReadMode>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_files::Limits>;
  using Auth = identity::AuthController<Exchange, AccessT>;
  using Gate = identity::TokenGate<Exchange, AccessT>;
  using Files = files::FilesController<Exchange, LibraryT, FileT>;

  static void attach(LibraryT& library) {
    wiring() = &library;
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
    const bool known = mado::check(route <= rest_files::kCardFile);
    LibraryT* const library = wiring();
    if (!(known && mado::check(library != nullptr))) {
      return exchange.send_empty(mado::http::kNotFound);
    }
    const bool pages = (route == rest_files::kOpenApiDocument) || (route == rest_files::kCardFile);
    CardT* const card = library->mounted();
    if (pages) {
      return rest_files::send_card<kReadMode>(exchange, card, route);
    }
    if (!mado::check(card != nullptr)) {
      return mado::send_problem(exchange, mado::Problem{
                                              mado::http::kServiceUnavailable,
                                              F("the SD card is not available"),
                                          });
    }
    switch (route) {
      case rest_files::kListFiles: return Files::list(exchange, *library);
      case rest_files::kGetFile: return Files::get(exchange, *library);
      case rest_files::kPutFile: return Files::put(exchange, *library);
      case rest_files::kDeleteFile: return Files::remove(exchange, *library);
      default: return exchange.send_empty(mado::http::kNotFound);
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
    if (route == rest_files::kLogin) {
      const bool answered = Auth::login(exchange, *keeper.access, now);
      static_cast<void>(answered);
      return false;
    }
    return (!rest_files::route_needs_token(route)) || Gate::admitted(exchange, *keeper.access, now);
  }

  static LibraryT*& wiring() {
    static LibraryT* instance = nullptr;
    return instance;
  }
};

}

#endif
