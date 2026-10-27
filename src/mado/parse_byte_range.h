// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PARSE_BYTE_RANGE_H
#define MADO_PARSE_BYTE_RANGE_H

#include <Arduino.h>
#include <stdint.h>

#include "ByteRange.h"
#include "check.h"
#include "to_lower_ascii.h"

namespace mado {

namespace detail {

constexpr uint8_t kRangeUnitLength = 6U;
constexpr uint8_t kMaxRangeDigits = 10U;
constexpr uint32_t kRangeDecimalBase = 10UL;

struct RangeNumber {
  uint32_t value;
  uint8_t digits;
  bool overflow;
};

inline RangeNumber read_range_number(const char* const text) {
  RangeNumber number = {0U, 0U, false};
  const bool valid = check(text != nullptr);
  for (uint8_t i = 0U; valid && (i <= kMaxRangeDigits); ++i) {
    const char symbol = text[i];
    if ((symbol < '0') || (symbol > '9')) {
      break;
    }
    const uint32_t digit = static_cast<uint32_t>(symbol - '0');
    const bool fits = number.value <= ((0xFFFFFFFFUL - digit) / kRangeDecimalBase);
    number.overflow = number.overflow || (!fits) || (i == kMaxRangeDigits);
    number.value = fits ? ((number.value * kRangeDecimalBase) + digit) : number.value;
    number.digits = static_cast<uint8_t>(i + 1U);
  }
  const bool counted = check(number.digits <= (kMaxRangeDigits + 1U));
  number.overflow = number.overflow || (!counted);
  return number;
}

inline bool range_unit(const char* const header) {
  const bool valid = check(header != nullptr);
  const char* const unit = PSTR("bytes=");
  bool same = valid;
  for (uint8_t i = 0U; same && (i < kRangeUnitLength); ++i) {
    same = to_lower_ascii(header[i]) == static_cast<char>(pgm_read_byte(unit + i));
  }
  return same && check(pgm_read_byte(unit + kRangeUnitLength) == 0U);
}

inline RangeStatus bounded_range(const RangeNumber& first, const RangeNumber& last,
                                 const uint32_t size, ByteRange& out) {
  const bool sound = check(size > 0U) && check(first.digits > 0U);
  if ((!sound) || first.overflow || (first.value >= size)) {
    return RangeStatus::kUnsatisfiable;
  }
  const bool open_ended = last.digits == 0U;
  if ((!open_ended) && (!last.overflow) && (last.value < first.value)) {
    return RangeStatus::kWhole;
  }
  const bool past_end = open_ended || last.overflow || (last.value >= size);
  out.first = first.value;
  out.last = past_end ? (size - 1U) : last.value;
  return RangeStatus::kPartial;
}

inline RangeStatus suffix_range(const RangeNumber& count, const uint32_t size, ByteRange& out) {
  const bool sound = check(size > 0U) && check(count.digits > 0U);
  if ((!sound) || ((count.value == 0U) && (!count.overflow))) {
    return RangeStatus::kUnsatisfiable;
  }
  const bool whole = count.overflow || (count.value >= size);
  out.first = whole ? 0U : (size - count.value);
  out.last = size - 1U;
  return RangeStatus::kPartial;
}

}

inline RangeStatus parse_byte_range(const char* const header, const uint32_t size, ByteRange& out) {
  out.first = 0U;
  out.last = (size > 0U) ? (size - 1U) : 0U;
  if ((header == nullptr) || (!detail::range_unit(header))) {
    return RangeStatus::kWhole;
  }
  const char* const spec = &header[detail::kRangeUnitLength];
  const detail::RangeNumber first = detail::read_range_number(spec);
  const bool dashed = spec[first.digits] == '-';
  const char* const rest = dashed ? &spec[first.digits + 1U] : spec;
  const detail::RangeNumber last = dashed ? detail::read_range_number(rest) : first;
  const bool ended = dashed && (rest[last.digits] == '\0');
  const bool shaped = ended && check(first.digits <= detail::kMaxRangeDigits + 1U) &&
                      ((first.digits > 0U) || (last.digits > 0U));
  if (!shaped) {
    return RangeStatus::kWhole;
  }
  if (size == 0U) {
    return RangeStatus::kUnsatisfiable;
  }
  const RangeStatus status = (first.digits == 0U) ? detail::suffix_range(last, size, out)
                                                  : detail::bounded_range(first, last, size, out);
  const bool ordered = check(out.first <= out.last) && check(out.last < size);
  return ordered ? status : RangeStatus::kWhole;
}

}

#endif
