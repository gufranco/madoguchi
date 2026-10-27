// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_PROBLEM_H
#define MADO_SEND_PROBLEM_H

#include <Arduino.h>
#include <stdint.h>

#include "FlashText.h"
#include "Problem.h"
#include "check.h"
#include "http_status.h"

namespace mado {

namespace detail {

template <typename ExchangeT>
bool write_problem_detail(ExchangeT& exchange, const __FlashStringHelper* const detail) {
  const bool open =
      check(exchange.status_sent()) && check(exchange.status_code() >= http::kBadRequest);
  if ((!open) || (detail == nullptr)) {
    return open;
  }
  const bool label = exchange.print(F(",\"detail\":\"")) > 0U;
  const bool text = label && (exchange.print(detail) > 0U);
  return text && (exchange.print(F("\"")) > 0U);
}

}

template <typename ExchangeT>
bool send_problem(ExchangeT& exchange, const Problem& problem) {
  const uint16_t code = problem.status;
  const bool valid = check(code >= http::kBadRequest) && check(code <= http::kLastValid);
  if ((!valid) || exchange.status_sent()) {
    return false;
  }
  const bool status = exchange.send_status(code);
  const bool typed = status && exchange.send_content_type(F("application/problem+json"));
  const bool sniffed =
      typed && exchange.send_header(F("X-Content-Type-Options"), FlashText(F("nosniff")));
  const bool cached = sniffed && exchange.send_header(F("Cache-Control"), FlashText(F("no-store")));
  const bool ended = cached && exchange.end_headers();
  bool written = ended && (exchange.print(F("{\"title\":\"")) > 0U);
  written = written && (exchange.print_reason(code) > 0U);
  written = written && (exchange.print(F("\",\"status\":")) > 0U);
  written = written && (exchange.print(static_cast<uint32_t>(code)) > 0U);
  written = written && detail::write_problem_detail(exchange, problem.detail);
  return written && (exchange.print(F("}")) > 0U);
}

}

#endif
