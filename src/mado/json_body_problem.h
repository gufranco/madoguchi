// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_BODY_PROBLEM_H
#define MADO_JSON_BODY_PROBLEM_H

#include <Arduino.h>
#include <stdint.h>

#include "Problem.h"
#include "check.h"
#include "http_status.h"
#include "is_json_media_type.h"

namespace mado {

template <typename ExchangeT>
Problem json_body_problem(const ExchangeT& exchange, const uint32_t max_body) {
  const bool fresh = check(!exchange.status_sent()) && check(max_body > 0U);
  if (!fresh) {
    return Problem{http::kInternalServerError, nullptr};
  }
  if (!is_json_media_type(exchange.header(F("Content-Type")))) {
    return Problem{http::kUnsupportedMediaType, F("Content-Type must be application/json")};
  }
  const uint32_t length = exchange.content_length();
  if (length == 0U) {
    return Problem{http::kBadRequest, F("a JSON body is required")};
  }
  if (length > max_body) {
    return Problem{http::kContentTooLarge, F("the body is larger than this operation accepts")};
  }
  return Problem{http::kNoResponse, nullptr};
}

}

#endif
