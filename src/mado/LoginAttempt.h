// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LOGIN_ATTEMPT_H
#define MADO_LOGIN_ATTEMPT_H

#include <stdint.h>

namespace mado {

struct LoginAttempt {
  uint32_t address;
  uint32_t now;
};

}

#endif
