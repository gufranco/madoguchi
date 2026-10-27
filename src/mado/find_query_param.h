// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FIND_QUERY_PARAM_H
#define MADO_FIND_QUERY_PARAM_H

#include <stdint.h>

#include "DecodeMode.h"
#include "check.h"
#include "kMaxStringScan.h"
#include "url_decode.h"

namespace mado {

namespace detail {

inline uint16_t segment_end(const char* const text, const uint16_t start) {
  const bool valid = check(text != nullptr) && check(start < kMaxStringScan);
  if (!valid) {
    return start;
  }
  uint16_t end = start;
  for (uint16_t i = start; i < kMaxStringScan; ++i) {
    end = i;
    const char symbol = text[i];
    const bool stop = (symbol == '&') || (symbol == '\0');
    if (stop) {
      break;
    }
  }
  return end;
}

inline uint16_t find_equals(const char* const text, const uint16_t start, const uint16_t end) {
  const bool valid = check(text != nullptr) && check(start <= end);
  if (!valid) {
    return end;
  }
  uint16_t found = end;
  for (uint16_t i = start; i < end; ++i) {
    const bool is_equals = text[i] == '=';
    if (is_equals) {
      found = i;
      break;
    }
  }
  return found;
}

inline bool name_equals(const char* const text, const uint16_t start, const uint16_t stop,
                        const char* const name) {
  const bool valid = check(start <= stop) && check(name != nullptr);
  if (!valid) {
    return false;
  }
  const uint16_t length = static_cast<uint16_t>(stop - start);
  bool equal = true;
  for (uint16_t i = 0U; i < length; ++i) {
    const char expected = name[i];
    const char actual = text[start + i];
    if ((expected == '\0') || (expected != actual)) {
      equal = false;
      break;
    }
  }
  return equal && (name[length] == '\0');
}

}

inline bool find_query_param(const char* const query, const char* const name, char* const out,
                             const uint16_t out_size) {
  const bool valid = check(query != nullptr) && check(name != nullptr) && check(out != nullptr) &&
                     check(out_size > 0U);
  if (!valid) {
    return false;
  }
  out[0] = '\0';
  uint16_t start = 0U;
  for (uint16_t guard = 0U; guard < kMaxStringScan; ++guard) {
    const uint16_t end = detail::segment_end(query, start);
    const uint16_t equals = detail::find_equals(query, start, end);
    const bool match = detail::name_equals(query, start, equals, name);
    if (match) {
      const uint16_t value_start = (equals < end) ? static_cast<uint16_t>(equals + 1U) : end;
      const uint16_t value_length = static_cast<uint16_t>(end - value_start);
      const int16_t decoded =
          url_decode(query + value_start, value_length, out, out_size, DecodeMode::kForm);
      return decoded >= 0;
    }
    const bool last = (query[end] == '\0') || (end >= (kMaxStringScan - 1U));
    if (last) {
      break;
    }
    start = static_cast<uint16_t>(end + 1U);
  }
  return false;
}

}

#endif
