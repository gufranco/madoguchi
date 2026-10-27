// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PBKDF2_PARAMS_H
#define MADO_PBKDF2_PARAMS_H

#include <stdint.h>

namespace mado {

struct Pbkdf2Params {
  const uint8_t* password;
  uint8_t password_length;
  const uint8_t* salt;
  uint8_t salt_length;
  uint32_t iterations;
};

}

#endif
