// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FILE_REPLY_H
#define MADO_FILE_REPLY_H

#include <Arduino.h>
#include <stdint.h>

namespace mado {

struct FileReply {
  uint32_t size;
  const __FlashStringHelper* type;
  bool gzip;
  const char* etag;
  const char* download_name;
  const __FlashStringHelper* cache_control;
};

}

#endif
