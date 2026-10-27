// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_LOG_ENTRY_H
#define MADO_LOG_ENTRY_H

#include <Arduino.h>
#include <stdint.h>

#include "Method.h"

namespace mado {

constexpr uint8_t kLogInfo = 30U;
constexpr uint8_t kLogWarn = 40U;
constexpr uint8_t kLogError = 50U;

struct LogStamp {
  uint32_t boot;
  uint32_t uptime_ms;
  const __FlashStringHelper* hostname;
};

struct RequestLog {
  uint32_t id;
  Method method;
  const char* url;
  uint16_t status;
  uint32_t response_ms;
};

}

#endif
