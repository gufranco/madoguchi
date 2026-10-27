// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FORM_STATUS_H
#define MADO_FORM_STATUS_H

#include <stdint.h>

namespace mado {

enum class FormStatus : uint8_t { kField, kEnd, kTooLong, kBroken };

}

#endif
