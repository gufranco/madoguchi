// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_REPLACE_FILE_H
#define MADO_REPLACE_FILE_H

#include <stdint.h>
#include <string.h>

#include "FileLength.h"
#include "check.h"
#include "copy_file.h"
#include "file_length.h"

namespace mado {

template <uint16_t kReadMode, uint16_t kWriteMode, typename SdT>
bool replace_file(SdT& card, const char* const source, const char* const target) {
  const bool valid =
      check(source != nullptr) && check(target != nullptr) && check(strcmp(source, target) != 0);
  if ((!valid) || (!card.exists(source))) {
    return false;
  }
  const bool cleared = (!card.exists(target)) || card.remove(target);
  auto input = card.open(source, kReadMode);
  auto output = card.open(target, kWriteMode);
  const bool opened = cleared && static_cast<bool>(input) && static_cast<bool>(output);
  const FileLength length = opened ? file_length(input) : FileLength{false, 0U};
  const bool copied = length.fits && copy_file(input, output, length.bytes);
  input.close();
  output.close();
  const bool removed = card.remove(source);
  return copied && check(removed || (!copied));
}

}

#endif
