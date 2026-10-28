// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_PERCENT_ENCODING_H
#define REST_FILES_FILES_PERCENT_ENCODING_H

#include <mado/check.h>
#include <mado/valid_card_name.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../domain/file_path.value.h"

namespace files {

constexpr uint8_t kLocationSize = 208U;
constexpr uint8_t kNibbleBits = 4U;
constexpr uint8_t kNibbleMask = 0x0FU;
constexpr uint8_t kDecimalDigits = 10U;
constexpr uint8_t kHexDigits = 16U;

inline bool unreserved(const char symbol) {
  const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
  const bool digit = (symbol >= '0') && (symbol <= '9');
  return mado::check(symbol != '\0') && (alpha || digit || (symbol == '-') || (symbol == '.') ||
                                         (symbol == '_') || (symbol == '~'));
}

inline char hex_digit(const uint8_t nibble) {
  const bool valid = mado::check(nibble < kHexDigits);
  const uint8_t base = (nibble < kDecimalDigits) ? static_cast<uint8_t>('0')
                                                 : static_cast<uint8_t>('A' - kDecimalDigits);
  return valid ? static_cast<char>(base + nibble) : '0';
}

inline bool encode_path(const FilePath& path, char (&location)[kLocationSize]) {
  const bool valid = mado::check(is_file_path(path)) && mado::check(location[0] == '/');
  size_t used = valid ? strnlen(&location[0], kLocationSize) : kLocationSize;
  for (uint8_t i = 1U; valid && (i < kPathSize) && (path.text[i] != '\0'); ++i) {
    const char symbol = path.text[i];
    const uint8_t code = static_cast<uint8_t>(symbol);
    const bool plain = unreserved(symbol);
    const size_t width = plain ? 1U : 3U;
    if ((used + width) >= kLocationSize) {
      return false;
    }
    if (plain) {
      location[used] = symbol;
    } else {
      location[used] = '%';
      location[used + 1U] = hex_digit(static_cast<uint8_t>(code >> kNibbleBits));
      location[used + 2U] = hex_digit(static_cast<uint8_t>(code & kNibbleMask));
    }
    used += width;
  }
  return valid && mado::check(used < kLocationSize);
}

}

#endif
