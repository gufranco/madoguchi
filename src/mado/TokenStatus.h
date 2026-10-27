// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_TOKEN_STATUS_H
#define MADO_TOKEN_STATUS_H

#include <stdint.h>

namespace mado {

enum class TokenStatus : uint8_t {
  kValid,
  kMissing,
  kMalformed,
  kBadSignature,
  kExpired,
  kStaleSession,
  kUnprovisioned,
};

}

#endif
