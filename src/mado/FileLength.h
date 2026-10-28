// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_LENGTH_STRUCT_H
#define MADO_FILE_LENGTH_STRUCT_H

#include <stdint.h>

namespace mado {

struct FileLength {
  bool fits;
  uint32_t bytes;
};

}

#endif
