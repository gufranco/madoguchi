// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_LENGTH_H
#define MADO_FILE_LENGTH_H

#include <stdint.h>

#include "FileLength.h"
#include "check.h"

namespace mado {

constexpr uint64_t kMaxFileLength = 0xFFFFFFFFULL;

template <typename FileT>
FileLength file_length(FileT& file) {
  const bool open = check(static_cast<bool>(file)) && check(!file.isDirectory());
  const uint64_t full = open ? static_cast<uint64_t>(file.size()) : 0U;
  const bool fits = open && (full <= kMaxFileLength);
  return FileLength{fits, fits ? static_cast<uint32_t>(full) : 0U};
}

}

#endif
