// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_DECODED_BYTE_H
#define MADO_DECODED_BYTE_H

#include <stdint.h>

namespace mado {

struct DecodedByte {
  char value;
  uint8_t consumed;
};

}

#endif
