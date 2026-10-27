// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HTTP_VERSION_H
#define MADO_HTTP_VERSION_H

#include <stdint.h>

namespace mado {

enum class HttpVersion : uint8_t { kUnsupported, kHttp10, kHttp11 };

}

#endif
