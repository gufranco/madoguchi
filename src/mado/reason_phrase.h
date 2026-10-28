// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_REASON_PHRASE_H
#define MADO_REASON_PHRASE_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "http_status.h"

namespace mado {

inline const char* reason_phrase(const uint16_t code) {
  const bool valid = check(code >= http::kFirstValid) && check(code <= http::kLastValid);
  if (!valid) {
    return PSTR("");
  }
  switch (code) {
    case http::kContinue: return PSTR("Continue");
    case http::kOk: return PSTR("OK");
    case http::kCreated: return PSTR("Created");
    case http::kAccepted: return PSTR("Accepted");
    case http::kNoContent: return PSTR("No Content");
    case http::kPartialContent: return PSTR("Partial Content");
    case http::kMovedPermanently: return PSTR("Moved Permanently");
    case http::kFound: return PSTR("Found");
    case http::kSeeOther: return PSTR("See Other");
    case http::kNotModified: return PSTR("Not Modified");
    case http::kBadRequest: return PSTR("Bad Request");
    case http::kUnauthorized: return PSTR("Unauthorized");
    case http::kForbidden: return PSTR("Forbidden");
    case http::kNotFound: return PSTR("Not Found");
    case http::kMethodNotAllowed: return PSTR("Method Not Allowed");
    case http::kNotAcceptable: return PSTR("Not Acceptable");
    case http::kRequestTimeout: return PSTR("Request Timeout");
    case http::kConflict: return PSTR("Conflict");
    case http::kLengthRequired: return PSTR("Length Required");
    case http::kPreconditionFailed: return PSTR("Precondition Failed");
    case http::kContentTooLarge: return PSTR("Content Too Large");
    case http::kUriTooLong: return PSTR("URI Too Long");
    case http::kUnsupportedMediaType: return PSTR("Unsupported Media Type");
    case http::kRangeNotSatisfiable: return PSTR("Range Not Satisfiable");
    case http::kUnprocessableContent: return PSTR("Unprocessable Content");
    case http::kTooManyRequests: return PSTR("Too Many Requests");
    case http::kHeaderFieldsTooLarge: return PSTR("Request Header Fields Too Large");
    case http::kInternalServerError: return PSTR("Internal Server Error");
    case http::kNotImplemented: return PSTR("Not Implemented");
    case http::kServiceUnavailable: return PSTR("Service Unavailable");
    case http::kHttpVersionNotSupported: return PSTR("HTTP Version Not Supported");
    case http::kInsufficientStorage: return PSTR("Insufficient Storage");
    default: return PSTR("");
  }
}

}

#endif
