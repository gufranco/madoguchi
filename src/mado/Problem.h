// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_PROBLEM_H
#define MADO_PROBLEM_H

#include <Arduino.h>
#include <stdint.h>

namespace mado {

struct Problem {
  uint16_t status;
  const __FlashStringHelper* detail;
};

}

#endif
