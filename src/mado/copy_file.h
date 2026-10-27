// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_COPY_FILE_H
#define MADO_COPY_FILE_H

#include <stddef.h>
#include <stdint.h>

#include "check.h"

namespace mado {

constexpr uint16_t kCopyChunkSize = 64U;
constexpr uint32_t kMaxCopyChunks = 0x04000000UL;

template <typename FileT>
bool copy_file(FileT& from, FileT& target, const uint32_t size) {
  const bool open = check(static_cast<bool>(from)) && check(static_cast<bool>(target));
  uint8_t chunk[kCopyChunkSize] = {};
  uint32_t remaining = size;
  for (uint32_t step = 0U; open && (remaining > 0U) && (step < kMaxCopyChunks); ++step) {
    const uint16_t wanted =
        static_cast<uint16_t>((remaining < kCopyChunkSize) ? remaining : kCopyChunkSize);
    const int32_t got = from.read(&chunk[0], wanted);
    if ((got <= 0) || (got > static_cast<int32_t>(wanted))) {
      return false;
    }
    const size_t written = target.write(&chunk[0], static_cast<size_t>(got));
    if (written != static_cast<size_t>(got)) {
      return false;
    }
    remaining -= static_cast<uint32_t>(got);
  }
  const bool bounded = check(remaining <= size);
  return open && bounded && (remaining == 0U);
}

}

#endif
