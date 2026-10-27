// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_SOURCE_H
#define MADO_FILE_SOURCE_H

#include <stdint.h>

#include "check.h"

namespace mado {

template <typename FileT>
class FileSource {
 public:
  explicit FileSource(FileT& file) : file_(&file), ended_(!check(static_cast<bool>(file))) {
  }

  int16_t next() {
    const bool wired = check(file_ != nullptr);
    if ((!wired) || ended_) {
      return -1;
    }
    uint8_t value = 0U;
    const int32_t got = file_->read(&value, 1U);
    ended_ = got != 1;
    const bool sane = check((got == 1) || (got <= 0));
    return (ended_ || (!sane)) ? static_cast<int16_t>(-1) : static_cast<int16_t>(value);
  }

 private:
  FileT* file_;
  bool ended_;
};

}

#endif
