// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_PROBLEM_H
#define MADO_JSON_PROBLEM_H

#include <Arduino.h>

#include "JsonStatus.h"
#include "Problem.h"
#include "check.h"
#include "http_status.h"
#include "json_error_status.h"

namespace mado {

inline Problem json_problem(const JsonStatus status) {
  const bool failed = check(status != JsonStatus::kOk) && check(status != JsonStatus::kEnd);
  if (!failed) {
    return Problem{http::kInternalServerError, nullptr};
  }
  const uint16_t code = json_error_status(status);
  const bool client_error = check(code >= http::kBadRequest) && (code < http::kInternalServerError);
  if (!client_error) {
    return Problem{http::kInternalServerError, nullptr};
  }
  switch (status) {
    case JsonStatus::kTooLong: return Problem{code, F("a string is longer than its maxLength")};
    case JsonStatus::kOutOfRange: return Problem{code, F("a number is outside its range")};
    case JsonStatus::kWrongType: return Problem{code, F("a property has the wrong type")};
    case JsonStatus::kUnsupported:
      return Problem{code, F("a property is unknown, read-only or repeated")};
    default: return Problem{code, F("the body is not valid JSON")};
  }
}

}

#endif
