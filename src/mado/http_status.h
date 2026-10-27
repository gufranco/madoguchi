// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HTTP_STATUS_H
#define MADO_HTTP_STATUS_H

#include <stdint.h>

namespace mado {

namespace http {

constexpr uint16_t kNoResponse = 0U;
constexpr uint16_t kFirstValid = 100U;
constexpr uint16_t kLastValid = 599U;

constexpr uint16_t kContinue = 100U;
constexpr uint16_t kOk = 200U;
constexpr uint16_t kCreated = 201U;
constexpr uint16_t kNoContent = 204U;
constexpr uint16_t kPartialContent = 206U;
constexpr uint16_t kMovedPermanently = 301U;
constexpr uint16_t kFound = 302U;
constexpr uint16_t kSeeOther = 303U;
constexpr uint16_t kNotModified = 304U;
constexpr uint16_t kBadRequest = 400U;
constexpr uint16_t kUnauthorized = 401U;
constexpr uint16_t kForbidden = 403U;
constexpr uint16_t kNotFound = 404U;
constexpr uint16_t kMethodNotAllowed = 405U;
constexpr uint16_t kNotAcceptable = 406U;
constexpr uint16_t kRequestTimeout = 408U;
constexpr uint16_t kLengthRequired = 411U;
constexpr uint16_t kContentTooLarge = 413U;
constexpr uint16_t kUriTooLong = 414U;
constexpr uint16_t kUnsupportedMediaType = 415U;
constexpr uint16_t kRangeNotSatisfiable = 416U;
constexpr uint16_t kUnprocessableContent = 422U;
constexpr uint16_t kTooManyRequests = 429U;
constexpr uint16_t kHeaderFieldsTooLarge = 431U;
constexpr uint16_t kInternalServerError = 500U;
constexpr uint16_t kNotImplemented = 501U;
constexpr uint16_t kServiceUnavailable = 503U;
constexpr uint16_t kHttpVersionNotSupported = 505U;
constexpr uint16_t kInsufficientStorage = 507U;

}

}

#endif
