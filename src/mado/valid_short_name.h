// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_VALID_SHORT_NAME_H
#define MADO_VALID_SHORT_NAME_H

#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint8_t kShortNameBase = 8U;
constexpr uint8_t kShortNameExtension = 3U;
constexpr uint8_t kShortNameSize = 13U;

inline bool valid_short_name(const char* const name) {
  const bool present = check(name != nullptr);
  uint8_t base = 0U;
  uint8_t extension = 0U;
  bool dotted = false;
  bool valid = present && (name[0] != '\0') && (name[0] != '.');
  for (uint8_t i = 0U; valid && (i < kShortNameSize); ++i) {
    const char symbol = name[i];
    if (symbol == '\0') {
      break;
    }
    const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
    const bool digit = (symbol >= '0') && (symbol <= '9');
    const bool mark = (symbol == '_') || (symbol == '-') || (symbol == '~');
    const bool dot = (symbol == '.') && (!dotted);
    dotted = dotted || dot;
    base = static_cast<uint8_t>(base + ((dotted || dot) ? 0U : 1U));
    extension = static_cast<uint8_t>(extension + ((dotted && (!dot)) ? 1U : 0U));
    valid = (alpha || digit || mark || dot) && (base <= kShortNameBase) &&
            (extension <= kShortNameExtension);
  }
  const bool ended = valid && (name[(base + extension) + (dotted ? 1U : 0U)] == '\0');
  return ended && check(base > 0U) && ((!dotted) || (extension > 0U));
}

}

#endif
