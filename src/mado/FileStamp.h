// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_STAMP_H
#define MADO_FILE_STAMP_H

#include <stdint.h>

namespace mado {

struct FileStamp {
  uint32_t size;
  uint32_t version;
};

}

#endif
