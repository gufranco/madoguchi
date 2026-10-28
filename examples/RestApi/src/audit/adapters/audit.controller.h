// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_AUDIT_AUDIT_CONTROLLER_H
#define REST_API_AUDIT_AUDIT_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/FileLength.h>
#include <mado/FileReply.h>
#include <mado/Problem.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_file.h>
#include <mado/send_problem.h>
#include <stdint.h>

namespace audit {

template <typename ExchangeT, typename JournalT>
class AuditController {
 public:
  static bool read(ExchangeT& exchange, JournalT& journal) {
    typename JournalT::FileT file;
    const mado::FileLength length = journal.open(file);
    if (!(mado::check(length.fits) && mado::check(static_cast<bool>(file)))) {
      file.close();
      return storage_missing(exchange);
    }
    const mado::FileReply reply = {
        length.bytes, F("application/x-ndjson"), false, nullptr, nullptr, F("no-store"),
    };
    const bool sent = mado::send_file(exchange, file, reply);
    file.close();
    return sent;
  }

  static bool clear(ExchangeT& exchange, JournalT& journal) {
    const bool cleared = mado::check(!exchange.status_sent()) && journal.clear();
    return mado::check(cleared) ? exchange.send_empty(mado::http::kNoContent)
                                : storage_missing(exchange);
  }

 private:
  static bool storage_missing(ExchangeT& exchange) {
    const bool fresh = mado::check(!exchange.status_sent());
    return fresh && mado::send_problem(exchange, mado::Problem{
                                                     mado::http::kServiceUnavailable,
                                                     F("the SD card is not available"),
                                                 });
  }
};

}

#endif
