// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_VALID_CARD_NAME_H
#define MADO_VALID_CARD_NAME_H

#include <stdint.h>
#include <string.h>

#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

constexpr uint8_t kCardNameSize = 65U;

namespace detail {

inline bool card_name_char(const char symbol) {
  const uint8_t code = static_cast<uint8_t>(symbol);
  const bool printable = check(code != 0U) && (code >= 0x20U) && (code <= 0x7EU);
  const bool reserved = (symbol == '"') || (symbol == '*') || (symbol == '/') || (symbol == ':') ||
                        (symbol == '<') || (symbol == '>') || (symbol == '?') || (symbol == '\\') ||
                        (symbol == '|') || (symbol == '%');
  return printable && (!reserved);
}

}

inline bool valid_card_name(const char* const name) {
  const bool present =
      check(name != nullptr) && check(strnlen(name, kMaxStringScan) < kMaxStringScan);
  bool valid = present && (name[0] != '\0') && (name[0] != '.') && (name[0] != ' ');
  uint8_t length = 0U;
  for (uint8_t i = 0U; valid && (i < kCardNameSize); ++i) {
    if (name[i] == '\0') {
      break;
    }
    valid = detail::card_name_char(name[i]);
    length = static_cast<uint8_t>(i + 1U);
  }
  const bool ended = valid && (length < kCardNameSize) && (name[length] == '\0');
  const char last = ended ? name[length - 1U] : '.';
  return ended && (last != '.') && (last != ' ');
}

}

#endif
