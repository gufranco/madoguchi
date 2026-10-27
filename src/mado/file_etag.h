// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_ETAG_H
#define MADO_FILE_ETAG_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FileStamp.h"
#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kFileEtagSize = 22U;

namespace detail {

constexpr uint32_t kFnvOffset = 2166136261UL;
constexpr uint32_t kFnvPrime = 16777619UL;
constexpr uint8_t kHexDigits = 8U;
constexpr uint8_t kHashStart = 3U;
constexpr uint8_t kSizeStart = kHashStart + kHexDigits + 1U;
constexpr uint8_t kTagEnd = kSizeStart + kHexDigits;
static_assert((kTagEnd + 2U) == kFileEtagSize, "W/\"hash-size\" and its NUL fill the tag");

inline bool put_hex(const uint32_t value, char* const field) {
  const bool valid = check(field != nullptr);
  for (uint8_t i = 0U; valid && (i < kHexDigits); ++i) {
    const uint8_t nibble = static_cast<uint8_t>((value >> (28U - (4U * i))) & 0x0FU);
    field[i] = static_cast<char>((nibble < 10U) ? ('0' + nibble) : ('a' + (nibble - 10U)));
  }
  return valid;
}

inline uint32_t tag_hash(const char* const path, const size_t length, const FileStamp& stamp) {
  const bool bounded = check(path != nullptr) && check(length < kMaxStringScan);
  uint32_t hash = kFnvOffset;
  for (size_t i = 0U; bounded && (i < length); ++i) {
    hash = (hash ^ static_cast<uint8_t>(path[i])) * kFnvPrime;
  }
  for (uint8_t shift = 0U; shift < 32U; shift = static_cast<uint8_t>(shift + 8U)) {
    hash = (hash ^ static_cast<uint8_t>(stamp.version >> shift)) * kFnvPrime;
  }
  return hash;
}

}

inline bool file_etag(const char* const path, const FileStamp& stamp, char (&out)[kFileEtagSize]) {
  const bool named = check(path != nullptr) && check(path[0] != '\0');
  const size_t length = named ? strnlen(path, kMaxStringScan) : kMaxStringScan;
  out[0] = '\0';
  if (!check(length < kMaxStringScan)) {
    return false;
  }
  out[0] = 'W';
  out[1] = '/';
  out[2] = '"';
  const bool hashed =
      detail::put_hex(detail::tag_hash(path, length, stamp), &out[detail::kHashStart]);
  out[detail::kSizeStart - 1U] = '-';
  const bool sized = detail::put_hex(stamp.size, &out[detail::kSizeStart]);
  out[detail::kTagEnd] = '"';
  out[detail::kTagEnd + 1U] = '\0';
  return hashed && sized;
}

}

#endif
