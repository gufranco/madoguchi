// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FIND_ROUTE_H
#define MADO_FIND_ROUTE_H

#include <stdint.h>

#include "Method.h"
#include "Route.h"
#include "RouteLookup.h"
#include "check.h"
#include "match_route.h"
#include "route_at.h"

namespace mado {

namespace detail {

inline bool method_accepts(const Method route_method, const Method request_method) {
  const bool route_ok = check(route_method != Method::kUnknown);
  const bool request_ok = check(request_method != Method::kAny);
  if (!(route_ok && request_ok)) {
    return false;
  }
  const bool any = route_method == Method::kAny;
  const bool same = route_method == request_method;
  const bool head_on_get = (route_method == Method::kGet) && (request_method == Method::kHead);
  return any || same || head_on_get;
}

}

template <uint8_t MaxRoutes>
RouteLookup find_route(const Route* const routes, const Method method, const char* const path) {
  RouteLookup lookup = {false, false, 0U};
  const bool valid = check(routes != nullptr) && check(path != nullptr);
  if (!valid) {
    return lookup;
  }
  for (uint8_t i = 0U; i < MaxRoutes; ++i) {
    const Route route = route_at(routes, i);
    if (route.path == nullptr) {
      break;
    }
    const bool path_ok = match_route(route.path, path);
    lookup.path_matched = lookup.path_matched || path_ok;
    const bool accepted = path_ok && detail::method_accepts(route.method, method);
    if (accepted) {
      lookup.found = true;
      lookup.id = route.id;
      break;
    }
  }
  return lookup;
}

}

#endif
