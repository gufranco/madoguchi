// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_GUARD_ENTRY_H
#define MADO_GUARD_ENTRY_H

#include <stdint.h>

namespace mado {

struct GuardEntry {
  uint32_t address;
  uint8_t failures;
};

}

#endif
