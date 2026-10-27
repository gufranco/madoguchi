// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_VALID_TOKEN_SUBJECT_H
#define MADO_VALID_TOKEN_SUBJECT_H

#include <stdint.h>

#include "check.h"
#include "kMaxTokenSubject.h"

namespace mado {

inline bool valid_token_subject(const char* const subject) {
  const bool present = check(subject != nullptr);
  if (!present) {
    return false;
  }
  uint8_t length = 0U;
  for (; length <= kMaxTokenSubject; ++length) {
    const char symbol = subject[length];
    if (symbol == '\0') {
      break;
    }
    const bool letter =
        ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
    const bool digit = (symbol >= '0') && (symbol <= '9');
    const bool mark = (symbol == '.') || (symbol == '_') || (symbol == '-');
    if (!(letter || digit || mark)) {
      return false;
    }
  }
  return (length > 0U) && (length <= kMaxTokenSubject);
}

}

#endif
