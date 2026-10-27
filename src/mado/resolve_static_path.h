// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_RESOLVE_STATIC_PATH_H
#define MADO_RESOLVE_STATIC_PATH_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "DecodeMode.h"
#include "check.h"
#include "flash_address.h"
#include "kMaxStringScan.h"
#include "url_decode.h"

namespace mado {

namespace detail {

inline bool segment_is_safe(const char* const text, const uint16_t start, const uint16_t end) {
  const bool valid = check(text != nullptr) && check(start <= end);
  if (!valid) {
    return false;
  }
  const uint16_t length = static_cast<uint16_t>(end - start);
  const bool dot = (length == 1U) && (text[start] == '.');
  const bool dot_dot = (length == 2U) && (text[start] == '.') && (text[start + 1U] == '.');
  return (!dot) && (!dot_dot);
}

inline bool decoded_path_is_safe(const char* const text, const uint16_t length) {
  const bool valid = check(text != nullptr) && check(length < kMaxStringScan);
  const bool rooted = valid && (length > 0U) && (text[0] == '/');
  if (!rooted) {
    return false;
  }
  uint16_t segment_start = 1U;
  for (uint16_t i = 1U; i <= length; ++i) {
    const char symbol = (i < length) ? text[i] : '/';
    if ((symbol == '\\') || (symbol == '\0')) {
      return false;
    }
    if (symbol != '/') {
      continue;
    }
    const bool empty_inside = (i == segment_start) && (i < length);
    const bool safe = segment_is_safe(text, segment_start, i);
    if (empty_inside || (!safe)) {
      return false;
    }
    segment_start = static_cast<uint16_t>(i + 1U);
  }
  return true;
}

inline bool append_text(char* const out, const uint16_t size, uint16_t& used,
                        const char* const text, const uint16_t length) {
  const bool valid = check(out != nullptr) && check((text != nullptr) || (length == 0U));
  const bool fits = valid && ((static_cast<uint32_t>(used) + length) < size);
  if (!fits) {
    return false;
  }
  memcpy(&out[used], text, length);
  used = static_cast<uint16_t>(used + length);
  out[used] = '\0';
  return true;
}

inline bool append_flash(char* const out, const uint16_t size, uint16_t& used,
                         const char* const flash) {
  const bool valid = check(out != nullptr) && check(flash != nullptr);
  if (!valid) {
    return false;
  }
  const uint16_t length = static_cast<uint16_t>(strlen_P(flash));
  const bool fits = (static_cast<uint32_t>(used) + length) < size;
  if (!fits) {
    return false;
  }
  memcpy_P(&out[used], flash, length);
  used = static_cast<uint16_t>(used + length);
  out[used] = '\0';
  return true;
}

inline bool append_decoded_path(char* const out, const uint16_t size, uint16_t& used,
                                const char* const url_path) {
  const bool valid = check(url_path != nullptr) && check(used < size);
  if (!valid) {
    return false;
  }
  const uint16_t path_length = static_cast<uint16_t>(strnlen(url_path, kMaxStringScan));
  const uint16_t room = static_cast<uint16_t>(size - used);
  const int16_t decoded = url_decode(url_path, path_length, &out[used], room, DecodeMode::kPath);
  if (decoded < 0) {
    return false;
  }
  const bool safe = decoded_path_is_safe(&out[used], static_cast<uint16_t>(decoded));
  used = static_cast<uint16_t>(used + static_cast<uint16_t>(decoded));
  return safe;
}

}

inline bool resolve_static_path(const char* const url_path, const char* const root,
                                const __FlashStringHelper* const index_name, char* const out,
                                const uint16_t size) {
  const bool valid = check(url_path != nullptr) && check(root != nullptr) &&
                     check(index_name != nullptr) && check(out != nullptr) && check(size > 1U);
  if (!valid) {
    return false;
  }
  out[0] = '\0';
  uint16_t used = 0U;
  const uint16_t root_length = static_cast<uint16_t>(strnlen(root, kMaxStringScan));
  const bool root_ok = detail::append_text(out, size, used, root, root_length);
  bool path_ok = false;
  if (root_ok) {
    path_ok = detail::append_decoded_path(out, size, used, url_path);
  }
  bool resolved = path_ok;
  const bool directory = path_ok && (used > 0U) && (out[used - 1U] == '/');
  if (directory) {
    resolved = detail::append_flash(out, size, used, flash_address(index_name));
  }
  if (!resolved) {
    out[0] = '\0';
  }
  return resolved;
}

}

#endif
