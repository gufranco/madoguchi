// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_BYTE_RANGE_H
#define MADO_BYTE_RANGE_H

#include <stdint.h>

namespace mado {

enum class RangeStatus : uint8_t { kWhole, kPartial, kUnsatisfiable };

struct ByteRange {
  uint32_t first;
  uint32_t last;
};

}

#endif
