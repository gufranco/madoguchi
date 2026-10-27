// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_NETWORK_ACTION_H
#define MADO_NETWORK_ACTION_H

#include <stdint.h>

namespace mado {

enum class NetworkAction : uint8_t { kNone, kRestart };

struct NetworkSample {
  bool hardware_present;
  bool link_known;
  bool link_up;
};

}

#endif
