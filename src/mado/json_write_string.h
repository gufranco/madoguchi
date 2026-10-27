// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_WRITE_STRING_H
#define MADO_JSON_WRITE_STRING_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

namespace detail {

template <typename ExchangeT>
size_t write_json_escape(ExchangeT& exchange, const uint8_t value) {
  const bool control = check((value < 0x20U) || (value == '"') || (value == '\\'));
  if (!control) {
    return 0U;
  }
  size_t written = exchange.write(static_cast<uint8_t>('\\'));
  if ((value == '"') || (value == '\\')) {
    written += exchange.write(value);
    return check(written <= 2U) ? written : 0U;
  }
  const char* const digits = PSTR("0123456789abcdef");
  written += exchange.write(static_cast<uint8_t>('u'));
  written += exchange.write(static_cast<uint8_t>('0'));
  written += exchange.write(static_cast<uint8_t>('0'));
  written += exchange.write(pgm_read_byte(digits + (value >> 4U)));
  written += exchange.write(pgm_read_byte(digits + (value & 0x0FU)));
  return written;
}

template <typename ExchangeT>
bool write_json_char(ExchangeT& exchange, const uint8_t value) {
  const bool present = check(value != 0U);
  if (!present) {
    return false;
  }
  const bool quoted = (value == '"') || (value == '\\');
  if (quoted || (value < 0x20U)) {
    const size_t expected = quoted ? 2U : 6U;
    return check(write_json_escape(exchange, value) == expected);
  }
  return exchange.write(value) == 1U;
}

}

template <typename ExchangeT>
bool json_write_string(ExchangeT& exchange, const char* const text) {
  const bool valid = check(text != nullptr);
  if (!valid) {
    return false;
  }
  bool complete = exchange.write(static_cast<uint8_t>('"')) == 1U;
  bool terminated = false;
  for (uint16_t i = 0U; complete && (i < kMaxStringScan); ++i) {
    const uint8_t value = static_cast<uint8_t>(text[i]);
    if (value == 0U) {
      terminated = true;
      break;
    }
    complete = detail::write_json_char(exchange, value);
  }
  complete = complete && check(terminated);
  return complete && (exchange.write(static_cast<uint8_t>('"')) == 1U);
}

}

#endif
