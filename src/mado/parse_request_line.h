// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_REQUEST_LINE_H
#define MADO_PARSE_REQUEST_LINE_H

#include <stdint.h>

#include "HttpVersion.h"
#include "Method.h"
#include "RequestLine.h"
#include "check.h"
#include "kMaxStringScan.h"
#include "parse_method.h"

namespace mado {

namespace detail {

constexpr uint16_t kMaxMethodLength = 0xFFU;
constexpr uint8_t kVersionPrefixLength = 5U;
static_assert(kVersionPrefixLength == (sizeof("HTTP/") - 1U), "the prefix length matches HTTP/");

inline bool parse_version(const char* const text, HttpVersion& version) {
  version = HttpVersion::kUnsupported;
  const bool valid = check(text != nullptr);
  if (!valid) {
    return false;
  }
  const bool named = (text[0] == 'H') && (text[1] == 'T') && (text[2] == 'T') && (text[3] == 'P') &&
                     (text[4] == '/');
  const char* const digits = named ? &text[kVersionPrefixLength] : text;
  const bool shaped = named && (digits[0] >= '0') && (digits[0] <= '9') && (digits[1] == '.') &&
                      (digits[2] >= '0') && (digits[2] <= '9') && (digits[3] == '\0');
  if (!shaped) {
    return false;
  }
  const HttpVersion minor = (digits[2] == '0') ? HttpVersion::kHttp10 : HttpVersion::kHttp11;
  version = (digits[0] == '1') ? minor : HttpVersion::kUnsupported;
  return true;
}

inline uint16_t skip_spaces(const char* const line, const uint16_t from) {
  const bool valid = check(line != nullptr) && check(from < kMaxStringScan);
  if (!valid) {
    return from;
  }
  uint16_t index = from;
  for (uint16_t i = from; i < kMaxStringScan; ++i) {
    index = i;
    const bool is_space = line[i] == ' ';
    if (!is_space) {
      break;
    }
  }
  return index;
}

inline uint16_t find_token_end(const char* const line, const uint16_t from) {
  const bool valid = check(line != nullptr) && check(from < kMaxStringScan);
  if (!valid) {
    return from;
  }
  uint16_t index = from;
  for (uint16_t i = from; i < kMaxStringScan; ++i) {
    index = i;
    const char symbol = line[i];
    const bool ends = (symbol == ' ') || (symbol == '\0');
    if (ends) {
      break;
    }
  }
  return index;
}

inline bool split_version(char* const line, const uint16_t target_end, HttpVersion& version) {
  const bool valid = check(line != nullptr) && check(target_end < kMaxStringScan);
  if ((!valid) || (line[target_end] != ' ')) {
    return false;
  }
  line[target_end] = '\0';
  const uint16_t start = skip_spaces(line, static_cast<uint16_t>(target_end + 1U));
  return parse_version(&line[start], version);
}

inline const char* split_query(char* const target) {
  const bool valid = check(target != nullptr) && check(target[0] == '/');
  if (!valid) {
    return "";
  }
  const char* query = "";
  for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
    const char symbol = target[i];
    if (symbol == '\0') {
      query = &target[i];
      break;
    }
    if (symbol == '?') {
      target[i] = '\0';
      query = &target[i + 1U];
      break;
    }
  }
  return query;
}

}

inline RequestLine parse_request_line(char* const line) {
  RequestLine result = {Method::kUnknown, nullptr, nullptr, false, HttpVersion::kUnsupported};
  const bool valid = check(line != nullptr);
  if (!valid) {
    return result;
  }
  const uint16_t method_end = detail::find_token_end(line, 0U);
  const bool method_ok =
      (method_end > 0U) && (method_end <= detail::kMaxMethodLength) && (line[method_end] == ' ');
  if (!method_ok) {
    return result;
  }
  result.method = parse_method(line, static_cast<uint8_t>(method_end));
  const uint16_t target_start = detail::skip_spaces(line, method_end);
  const uint16_t target_end = detail::find_token_end(line, target_start);
  const bool target_ok = (target_start < target_end) && (line[target_start] == '/');
  if (!target_ok) {
    return result;
  }
  const bool versioned = detail::split_version(line, target_end, result.version);
  if (!versioned) {
    return result;
  }
  char* const target = &line[target_start];
  result.path = target;
  result.query = detail::split_query(target);
  result.valid = true;
  return result;
}

}

#endif
