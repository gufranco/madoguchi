// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ROUTE_AT_H
#define MADO_ROUTE_AT_H

#include <Arduino.h>
#include <stdint.h>

#include "Method.h"
#include "Route.h"
#include "check.h"

namespace mado {

inline Route route_at(const Route* const table, const uint8_t index) {
  Route route = {nullptr, Method::kAny, 0U};
  const bool valid = check(table != nullptr) && check(index < 0xFFU);
  if (valid) {
    static_cast<void>(memcpy_P(&route, &table[index], sizeof(route)));
  }
  return route;
}

}

#endif
