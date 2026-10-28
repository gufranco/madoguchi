// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ETAG_MATCHES_H
#define MADO_ETAG_MATCHES_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "flash_address.h"
#include "kMaxStringScan.h"

namespace mado {

namespace detail {

inline uint16_t skip_blanks(const char* const text, uint16_t index) {
  const bool valid = check(text != nullptr) && check(index < kMaxStringScan);
  while (valid && (index < kMaxStringScan) && ((text[index] == ' ') || (text[index] == '\t'))) {
    ++index;
  }
  return index;
}

inline char tag_char(const char* const tag, const size_t index, const bool flash) {
  const bool valid = check(tag != nullptr) && check(index < kMaxStringScan);
  if (!valid) {
    return '\0';
  }
  return flash ? static_cast<char>(pgm_read_byte(tag + index)) : tag[index];
}

inline bool tag_at(const char* const text, const uint16_t start, const char* const tag,
                   const bool flash, uint16_t& end) {
  const bool valid = check(text != nullptr) && check(tag != nullptr);
  end = start;
  if (!valid) {
    return false;
  }
  bool same = true;
  const size_t length = flash ? strlen_P(tag) : strlen(tag);
  for (size_t i = 0U; same && (i < length) && ((start + i) < kMaxStringScan); ++i) {
    same = text[start + i] == tag_char(tag, i, flash);
  }
  end = static_cast<uint16_t>(start + length);
  return same && (end < kMaxStringScan);
}

inline bool etag_in(const char* const header, const char* const tag, const bool flash,
                    const bool strong) {
  const bool valid = check(tag != nullptr);
  if ((!valid) || (header == nullptr)) {
    return false;
  }
  const size_t tag_length = flash ? strlen_P(tag) : strlen(tag);
  const bool short_tag = check(tag_length < kMaxStringScan);
  if (!short_tag) {
    return false;
  }
  uint16_t index = skip_blanks(header, 0U);
  if ((header[index] == '*') && (header[skip_blanks(header, index + 1U)] == '\0')) {
    return true;
  }
  for (uint8_t entry = 0U; (entry < UINT8_MAX) && (header[index] != '\0'); ++entry) {
    index = skip_blanks(header, index);
    const bool weak = (header[index] == 'W') && (header[index + 1U] == '/');
    index = static_cast<uint16_t>(index + (weak ? 2U : 0U));
    uint16_t end = index;
    const bool same = tag_at(header, index, tag, flash, end) && ((!strong) || (!weak));
    const uint16_t after = skip_blanks(header, end);
    if (same && ((header[after] == ',') || (header[after] == '\0'))) {
      return true;
    }
    while ((index < kMaxStringScan) && (header[index] != ',') && (header[index] != '\0')) {
      ++index;
    }
    index = static_cast<uint16_t>(index + ((header[index] == ',') ? 1U : 0U));
  }
  return false;
}

}

inline bool etag_matches(const char* const header, const __FlashStringHelper* const etag) {
  const bool valid = check(etag != nullptr);
  return valid && detail::etag_in(header, flash_address(etag), true, false);
}

inline bool etag_matches(const char* const header, const char* const etag) {
  const bool valid = check(etag != nullptr) && check(etag[0] != '\0');
  const bool weak = valid && (etag[0] == 'W') && (etag[1] == '/');
  return valid && detail::etag_in(header, weak ? &etag[2] : etag, false, false);
}

}

#endif
