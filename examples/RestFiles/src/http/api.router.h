// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_API_ROUTER_H
#define REST_FILES_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../files/adapters/files.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename LibraryT, typename CardT, typename FileT, uint16_t kReadMode>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_files::Limits>;
  using Files = files::FilesController<Exchange, LibraryT, FileT>;

  static void attach(LibraryT& library) {
    wiring() = &library;
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
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
  static LibraryT*& wiring() {
    static LibraryT* instance = nullptr;
    return instance;
  }
};

}

#endif
