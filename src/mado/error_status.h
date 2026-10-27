// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ERROR_STATUS_H
#define MADO_ERROR_STATUS_H

#include <stdint.h>

#include "HeaderStatus.h"
#include "LineStatus.h"
#include "http_status.h"

namespace mado {

static_assert(http::kUriTooLong >= http::kFirstValid, "error codes are valid statuses");
static_assert(http::kRequestTimeout >= http::kFirstValid, "error codes are valid statuses");
static_assert(http::kBadRequest >= http::kFirstValid, "error codes are valid statuses");
static_assert(http::kHeaderFieldsTooLarge <= http::kLastValid, "error codes are valid statuses");
static_assert(http::kNotImplemented <= http::kLastValid, "error codes are valid statuses");

inline uint16_t error_status(const LineStatus status) {
  uint16_t code = http::kNoResponse;
  if (status == LineStatus::kTooLong) {
    code = http::kUriTooLong;
  }
  if (status == LineStatus::kTimeout) {
    code = http::kRequestTimeout;
  }
  return code;
}

inline uint16_t error_status(const HeaderStatus status) {
  uint16_t code = http::kNoResponse;
  if (status == HeaderStatus::kTimeout) {
    code = http::kRequestTimeout;
  }
  if (status == HeaderStatus::kMalformed) {
    code = http::kBadRequest;
  }
  if (status == HeaderStatus::kTooMany) {
    code = http::kHeaderFieldsTooLarge;
  }
  if (status == HeaderStatus::kUnsupportedCoding) {
    code = http::kNotImplemented;
  }
  return code;
}

}

#endif
