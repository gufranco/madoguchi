// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_METHOD_H
#define MADO_PARSE_METHOD_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "Method.h"
#include "check.h"

namespace mado {

namespace detail {

inline bool token_is(const char* const token, const uint8_t length, const char* const flash) {
  const bool valid = check(token != nullptr) && check(flash != nullptr);
  if (!valid) {
    return false;
  }
  const bool same_length = strlen_P(flash) == length;
  if (!same_length) {
    return false;
  }
  const int32_t difference = strncmp_P(token, flash, length);
  return difference == 0;
}

}

inline Method parse_method(const char* const token, const uint8_t length) {
  const bool valid = check(token != nullptr) && check(length > 0U);
  if (!valid) {
    return Method::kUnknown;
  }
  Method method = Method::kUnknown;
  if (detail::token_is(token, length, PSTR("GET"))) {
    method = Method::kGet;
  } else if (detail::token_is(token, length, PSTR("HEAD"))) {
    method = Method::kHead;
  } else if (detail::token_is(token, length, PSTR("POST"))) {
    method = Method::kPost;
  } else if (detail::token_is(token, length, PSTR("PUT"))) {
    method = Method::kPut;
  } else if (detail::token_is(token, length, PSTR("DELETE"))) {
    method = Method::kDelete;
  } else if (detail::token_is(token, length, PSTR("PATCH"))) {
    method = Method::kPatch;
  } else if (detail::token_is(token, length, PSTR("OPTIONS"))) {
    method = Method::kOptions;
  }
  return method;
}

}

#endif
