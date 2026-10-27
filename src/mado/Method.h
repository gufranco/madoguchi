// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_METHOD_H
#define MADO_METHOD_H

#include <stdint.h>

namespace mado {

enum class Method : uint8_t { kUnknown, kGet, kHead, kPost, kPut, kDelete, kPatch, kOptions, kAny };

}

#endif
