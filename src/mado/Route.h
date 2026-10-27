// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ROUTE_H
#define MADO_ROUTE_H

#include <stdint.h>

#include "Method.h"

namespace mado {

struct Route {
  const char* path;
  Method method;
  uint8_t id;
};

}

#endif
