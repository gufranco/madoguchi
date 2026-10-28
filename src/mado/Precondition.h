// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PRECONDITION_H
#define MADO_PRECONDITION_H

#include <stdint.h>

namespace mado {

enum class Precondition : uint8_t {
  kAbsent,
  kHolds,
  kFails,
};

}

#endif
