// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_READ_STATUS_H
#define MADO_READ_STATUS_H

#include <stdint.h>

namespace mado {

enum class ReadStatus : uint8_t { kByte, kTimeout, kClosed };

}

#endif
