// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEADER_STATUS_H
#define MADO_HEADER_STATUS_H

#include <stdint.h>

namespace mado {

enum class HeaderStatus : uint8_t {
  kOk,
  kTimeout,
  kClosed,
  kMalformed,
  kTooMany,
  kUnsupportedCoding,
};

}

#endif
