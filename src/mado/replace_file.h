// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_REPLACE_FILE_H
#define MADO_REPLACE_FILE_H

#include <stdint.h>

#include "check.h"
#include "copy_file.h"

namespace mado {

template <uint8_t kReadMode, uint8_t kWriteMode, typename SdT>
bool replace_file(SdT& card, const char* const source, const char* const target) {
  const bool valid = check(source != nullptr) && check(target != nullptr);
  if ((!valid) || (!card.exists(source))) {
    return false;
  }
  const bool cleared = (!card.exists(target)) || card.remove(target);
  auto input = card.open(source, kReadMode);
  auto output = card.open(target, kWriteMode);
  const bool opened = cleared && static_cast<bool>(input) && static_cast<bool>(output);
  const bool copied = opened && copy_file(input, output, input.size());
  input.close();
  output.close();
  const bool removed = card.remove(source);
  return copied && check(removed || (!copied));
}

}

#endif
