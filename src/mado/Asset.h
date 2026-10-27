// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_ASSET_H
#define MADO_ASSET_H

#include <Arduino.h>
#include <stdint.h>

namespace mado {

struct Asset {
  const uint8_t* data;
  uint16_t size;
  const __FlashStringHelper* head;
  const __FlashStringHelper* revalidated;
  const __FlashStringHelper* etag;
};

}

#endif
