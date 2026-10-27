// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ROUTE_LOOKUP_H
#define MADO_ROUTE_LOOKUP_H

#include <stdint.h>

namespace mado {

struct RouteLookup {
  bool found;
  bool path_matched;
  uint8_t id;
};

}

#endif
