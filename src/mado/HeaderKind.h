// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEADER_KIND_H
#define MADO_HEADER_KIND_H

#include <stdint.h>

namespace mado {

enum class HeaderKind : uint8_t { kIgnored, kContentLength, kExpect, kCaptured };

}

#endif
