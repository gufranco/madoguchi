// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_IF_MATCH_H
#define MADO_IF_MATCH_H

#include <stdint.h>
#include <string.h>

#include "Precondition.h"
#include "check.h"
#include "etag_matches.h"
#include "kMaxStringScan.h"

namespace mado {

inline Precondition if_match(const char* const header, const char* const current_tag) {
  if (header == nullptr) {
    return Precondition::kAbsent;
  }
  const bool bounded = check(strnlen(header, kMaxStringScan) < kMaxStringScan);
  const bool current = (current_tag != nullptr) && check(current_tag[0] == '"');
  const bool holds = bounded && current && detail::etag_in(header, current_tag, false, true);
  return holds ? Precondition::kHolds : Precondition::kFails;
}

}

#endif
