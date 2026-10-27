// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_CONTENT_TYPE_FOR_H
#define MADO_CONTENT_TYPE_FOR_H

#include <Arduino.h>
#include <stdint.h>

#include "check.h"
#include "equals_ignore_case_P.h"
#include "kMaxStringScan.h"

namespace mado {

namespace detail {

inline const char* file_extension(const char* const name) {
  const bool valid = check(name != nullptr);
  if (!valid) {
    return nullptr;
  }
  const char* extension = nullptr;
  for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
    const char symbol = name[i];
    if (symbol == '\0') {
      break;
    }
    if (symbol == '.') {
      extension = &name[i + 1U];
    }
    if (symbol == '/') {
      extension = nullptr;
    }
  }
  return extension;
}

inline const __FlashStringHelper* text_type_for(const char* const extension) {
  const bool valid = check(extension != nullptr);
  if (!valid) {
    return nullptr;
  }
  const __FlashStringHelper* type = nullptr;
  if (equals_ignore_case_P(extension, PSTR("htm")) ||
      equals_ignore_case_P(extension, PSTR("html"))) {
    type = F("text/html; charset=utf-8");
  } else if (equals_ignore_case_P(extension, PSTR("css"))) {
    type = F("text/css");
  } else if (equals_ignore_case_P(extension, PSTR("js")) ||
             equals_ignore_case_P(extension, PSTR("mjs"))) {
    type = F("text/javascript");
  } else if (equals_ignore_case_P(extension, PSTR("json"))) {
    type = F("application/json");
  } else if (equals_ignore_case_P(extension, PSTR("txt"))) {
    type = F("text/plain; charset=utf-8");
  } else if (equals_ignore_case_P(extension, PSTR("xml"))) {
    type = F("text/xml");
  } else if (equals_ignore_case_P(extension, PSTR("svg"))) {
    type = F("image/svg+xml");
  }
  return type;
}

inline const __FlashStringHelper* binary_type_for(const char* const extension) {
  const __FlashStringHelper* type = F("application/octet-stream");
  const bool valid = check(extension != nullptr);
  if (!valid) {
    return type;
  }
  if (equals_ignore_case_P(extension, PSTR("png"))) {
    type = F("image/png");
  } else if (equals_ignore_case_P(extension, PSTR("jpg")) ||
             equals_ignore_case_P(extension, PSTR("jpeg"))) {
    type = F("image/jpeg");
  } else if (equals_ignore_case_P(extension, PSTR("gif"))) {
    type = F("image/gif");
  } else if (equals_ignore_case_P(extension, PSTR("ico"))) {
    type = F("image/vnd.microsoft.icon");
  } else if (equals_ignore_case_P(extension, PSTR("webp"))) {
    type = F("image/webp");
  } else if (equals_ignore_case_P(extension, PSTR("woff2"))) {
    type = F("font/woff2");
  } else if (equals_ignore_case_P(extension, PSTR("mp3"))) {
    type = F("audio/mpeg");
  } else if (equals_ignore_case_P(extension, PSTR("pdf"))) {
    type = F("application/pdf");
  }
  return type;
}

}

inline const __FlashStringHelper* content_type_for(const char* const file_name) {
  const bool valid = check(file_name != nullptr);
  if (!valid) {
    return F("application/octet-stream");
  }
  const char* const extension = detail::file_extension(file_name);
  const bool inside = (extension == nullptr) || check(extension > file_name);
  if ((extension == nullptr) || (!inside)) {
    return F("application/octet-stream");
  }
  const __FlashStringHelper* const text = detail::text_type_for(extension);
  return (text != nullptr) ? text : detail::binary_type_for(extension);
}

}

#endif
