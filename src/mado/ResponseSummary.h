// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_RESPONSE_SUMMARY_H
#define MADO_RESPONSE_SUMMARY_H

#include <stdint.h>

namespace mado {

struct ResponseSummary {
  uint16_t status;
  uint32_t started_ms;
};

}

#endif
