// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_ERROR_STATUS_H
#define MADO_JSON_ERROR_STATUS_H

#include <stdint.h>

#include "JsonStatus.h"
#include "check.h"
#include "http_status.h"

namespace mado {

inline uint16_t json_error_status(const JsonStatus status) {
  const bool failed = check(status != JsonStatus::kOk) && check(status != JsonStatus::kEnd);
  if (!failed) {
    return http::kInternalServerError;
  }
  const bool malformed = (status == JsonStatus::kSyntax) || (status == JsonStatus::kTruncated);
  return malformed ? http::kBadRequest : http::kUnprocessableContent;
}

}

#endif
