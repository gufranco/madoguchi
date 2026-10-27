// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_TOKEN_CLAIMS_H
#define MADO_TOKEN_CLAIMS_H

#include <stdint.h>

#include "kMaxTokenSubject.h"

namespace mado {

struct TokenClaims {
  char subject[kMaxTokenSubject + 1U];
  uint32_t session;
  uint32_t ends_at;
};

}

#endif
