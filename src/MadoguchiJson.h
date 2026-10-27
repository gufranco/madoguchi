// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADOGUCHI_JSON_H
#define MADOGUCHI_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "mado/Method.h"
#include "mado/check.h"
#include "mado/http_status.h"

namespace mado {

template <typename ExchangeT>
bool send_json(ExchangeT& exchange, const uint16_t code, const JsonDocument& document) {
  const bool valid = check(!exchange.status_sent()) && check(code >= http::kFirstValid) &&
                     check(code <= http::kLastValid);
  if (!valid) {
    return false;
  }
  const size_t length = measureJson(document);
  const bool status = exchange.send_status(code);
  const bool typed = exchange.send_content_type(F("application/json"));
  const bool sized = exchange.send_content_length(static_cast<uint32_t>(length));
  const bool ended = exchange.end_headers();
  const bool headers_ok = status && typed && sized && ended;
  if ((!headers_ok) || (exchange.method() == Method::kHead)) {
    return headers_ok;
  }
  const size_t written = serializeJson(document, exchange);
  return written == length;
}

}

#endif
