// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_TO_LOWER_ASCII_H
#define MADO_TO_LOWER_ASCII_H

namespace mado {

inline char to_lower_ascii(const char symbol) {
  const bool is_upper = (symbol >= 'A') && (symbol <= 'Z');
  char lower = symbol;
  if (is_upper) {
    lower = static_cast<char>(symbol + ('a' - 'A'));
  }
  return lower;
}

}

#endif
