// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LINE_STATUS_H
#define MADO_LINE_STATUS_H

#include <stdint.h>

namespace mado {

enum class LineStatus : uint8_t { kOk, kTooLong, kTimeout, kClosed };

}

#endif
