// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_NEXT_FORM_FIELD_H
#define MADO_NEXT_FORM_FIELD_H

#include <stdint.h>

#include "FormStatus.h"
#include "FormToken.h"
#include "check.h"
#include "hex_digit_value.h"

namespace mado {

namespace detail {

constexpr uint8_t kFormHexRadix = 16U;
constexpr uint16_t kMaxFormToken = 0x7FFFU;

enum class FormRead : uint8_t { kByte, kEnd, kBroken };

template <typename ExchangeT>
FormRead next_body_byte(ExchangeT& exchange, char& out) {
  uint8_t value = 0U;
  const int16_t got = exchange.read_body(&value, 1U);
  const bool bounded = check(got <= 1);
  if (!bounded) {
    return FormRead::kBroken;
  }
  if (got == 0) {
    return FormRead::kEnd;
  }
  if (got < 0) {
    return FormRead::kBroken;
  }
  out = static_cast<char>(value);
  return FormRead::kByte;
}

template <typename ExchangeT>
FormRead decode_escape(ExchangeT& exchange, char& out) {
  char high = '\0';
  char low = '\0';
  const FormRead first = next_body_byte(exchange, high);
  if (first != FormRead::kByte) {
    return FormRead::kBroken;
  }
  const FormRead second = next_body_byte(exchange, low);
  const int8_t high_value = hex_digit_value(high);
  const int8_t low_value = hex_digit_value(low);
  const bool valid = (second == FormRead::kByte) && (high_value >= 0) && (low_value >= 0);
  if (!valid) {
    return FormRead::kBroken;
  }
  const uint32_t decoded =
      (static_cast<uint32_t>(high_value) * kFormHexRadix) + static_cast<uint32_t>(low_value);
  out = static_cast<char>(decoded);
  return FormRead::kByte;
}

template <typename ExchangeT>
FormToken read_form_token(ExchangeT& exchange, const char stop, char* const out,
                          const uint16_t size) {
  const bool valid = check(out != nullptr) && check(size > 0U);
  if (!valid) {
    return FormToken{FormStatus::kBroken, false};
  }
  uint16_t length = 0U;
  for (uint32_t step = 0U; step <= kMaxFormToken; ++step) {
    char symbol = '\0';
    const FormRead raw = next_body_byte(exchange, symbol);
    if (raw == FormRead::kBroken) {
      return FormToken{FormStatus::kBroken, false};
    }
    const bool ends = (raw == FormRead::kEnd) || (symbol == '&') || (symbol == stop);
    if (ends) {
      out[length] = '\0';
      const bool at_separator = (raw == FormRead::kByte) && (symbol == stop) && (stop != '&');
      const bool empty_body = (raw == FormRead::kEnd) && (step == 0U);
      return FormToken{empty_body ? FormStatus::kEnd : FormStatus::kField, at_separator};
    }
    const bool escaped = symbol == '%';
    const FormRead decoded = escaped ? decode_escape(exchange, symbol) : FormRead::kByte;
    const bool full = (static_cast<uint32_t>(length) + 1U) >= size;
    if ((decoded != FormRead::kByte) || full) {
      return FormToken{full ? FormStatus::kTooLong : FormStatus::kBroken, false};
    }
    out[length] = ((!escaped) && (symbol == '+')) ? ' ' : symbol;
    ++length;
  }
  return FormToken{FormStatus::kTooLong, false};
}

}

template <typename ExchangeT>
FormStatus next_form_field(ExchangeT& exchange, char* const name, const uint16_t name_size,
                           char* const value, const uint16_t value_size) {
  const bool valid = check(name != nullptr) && check(value != nullptr) && check(name_size > 0U) &&
                     check(value_size > 0U);
  if (!valid) {
    return FormStatus::kBroken;
  }
  value[0] = '\0';
  const FormToken name_token = detail::read_form_token(exchange, '=', name, name_size);
  if ((name_token.status != FormStatus::kField) || (!name_token.stopped_at_separator)) {
    return name_token.status;
  }
  const FormToken value_token = detail::read_form_token(exchange, '&', value, value_size);
  return (value_token.status == FormStatus::kEnd) ? FormStatus::kField : value_token.status;
}

}

#endif
