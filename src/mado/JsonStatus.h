// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_STATUS_H
#define MADO_JSON_STATUS_H

#include <stdint.h>

namespace mado {

enum class JsonStatus : uint8_t {
  kOk,
  kEnd,
  kSyntax,
  kTooLong,
  kOutOfRange,
  kWrongType,
  kTruncated,
  kUnsupported,
};

}

#endif
