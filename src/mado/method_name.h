// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_METHOD_NAME_H
#define MADO_METHOD_NAME_H

#include <Arduino.h>

#include "Method.h"
#include "check.h"

namespace mado {

inline const __FlashStringHelper* method_name(const Method method) {
  const bool known = check(method != Method::kAny) &&
                     check(static_cast<uint8_t>(method) <= static_cast<uint8_t>(Method::kAny));
  if (!known) {
    return F("UNKNOWN");
  }
  switch (method) {
    case Method::kGet: return F("GET");
    case Method::kHead: return F("HEAD");
    case Method::kPost: return F("POST");
    case Method::kPut: return F("PUT");
    case Method::kDelete: return F("DELETE");
    case Method::kPatch: return F("PATCH");
    case Method::kOptions: return F("OPTIONS");
    default: return F("UNKNOWN");
  }
}

}

#endif
