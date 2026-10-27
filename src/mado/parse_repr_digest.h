// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_REPR_DIGEST_H
#define MADO_PARSE_REPR_DIGEST_H

#include <Arduino.h>
#include <stdint.h>

#include "Base64UrlDecoder.h"
#include "check.h"
#include "kMaxStringScan.h"
#include "kSha256Size.h"
#include "to_lower_ascii.h"

namespace mado {

namespace detail {

constexpr uint8_t kDigestKeyLength = 9U;
constexpr uint8_t kDigestTextLength = 44U;

inline uint16_t digest_start(const char* const header) {
  const bool valid = check(header != nullptr);
  const char* const key = PSTR("sha-256=:");
  bool at_entry = true;
  for (uint16_t i = 0U; valid && (i < kMaxStringScan) && (header[i] != '\0'); ++i) {
    bool same = at_entry;
    for (uint8_t k = 0U; same && (k < kDigestKeyLength); ++k) {
      const char wanted = static_cast<char>(pgm_read_byte(key + k));
      same = to_lower_ascii(header[i + k]) == wanted;
    }
    if (same) {
      return static_cast<uint16_t>(i + kDigestKeyLength);
    }
    const char symbol = header[i];
    at_entry = (symbol == ',') || (symbol == ' ') || ((symbol == '\t') && at_entry);
  }
  return kMaxStringScan;
}

inline bool take_group(const uint8_t (&group)[Base64UrlDecoder::kMaxOutput], const int8_t count,
                       uint8_t (&out)[kSha256Size], uint8_t& produced) {
  const bool valid = check(count >= 0) &&
                     check(count <= static_cast<int8_t>(Base64UrlDecoder::kMaxOutput)) &&
                     check(produced <= kSha256Size);
  const uint8_t size = valid ? static_cast<uint8_t>(count) : 0U;
  const bool fits = valid && ((produced + size) <= kSha256Size);
  for (uint8_t i = 0U; fits && (i < size); ++i) {
    out[produced] = group[i];
    ++produced;
  }
  return fits;
}

}

inline bool parse_repr_digest(const char* const header, uint8_t (&out)[kSha256Size]) {
  const uint16_t start = (header == nullptr) ? kMaxStringScan : detail::digest_start(header);
  if (start >= kMaxStringScan) {
    return false;
  }
  Base64UrlDecoder decoder;
  uint8_t group[Base64UrlDecoder::kMaxOutput] = {};
  uint8_t produced = 0U;
  bool valid = true;
  uint8_t index = 0U;
  for (; valid && (index < detail::kDigestTextLength); ++index) {
    const char symbol = header[start + index];
    if ((symbol == '=') || (symbol == ':') || (symbol == '\0')) {
      break;
    }
    const char dashed = (symbol == '+') ? '-' : symbol;
    const char mapped = (dashed == '/') ? '_' : dashed;
    const int8_t got = decoder.push(mapped, &group[0]);
    valid = detail::take_group(group, got, out, produced);
  }
  const bool bounded = check(index <= detail::kDigestTextLength);
  const int8_t tail = (valid && bounded) ? decoder.finish(&group[0]) : -1;
  const bool finished = valid && bounded && detail::take_group(group, tail, out, produced);
  return finished && (produced == kSha256Size);
}

}

#endif
