// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SERVE_CARD_FILE_H
#define MADO_SERVE_CARD_FILE_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "FileLength.h"
#include "FlashText.h"
#include "Method.h"
#include "accepts_gzip.h"
#include "check.h"
#include "content_type_for.h"
#include "file_length.h"
#include "http_status.h"
#include "kMaxStringScan.h"
#include "resolve_static_path.h"
#include "send_file.h"

namespace mado {

constexpr uint8_t kCardPathSize = 128U;
constexpr uint8_t kCardHashLength = 8U;

namespace detail {

inline bool hex_digit(const char symbol) {
  const bool present = check(symbol != '\0');
  return present && (((symbol >= '0') && (symbol <= '9')) || ((symbol >= 'a') && (symbol <= 'f')) ||
                     ((symbol >= 'A') && (symbol <= 'F')));
}

inline bool hashed_card_name(const char* const path) {
  const bool valid = check(path != nullptr) && check(strnlen(path, kCardPathSize) < kCardPathSize);
  const char* const slash = valid ? strrchr(path, '/') : nullptr;
  const char* const name = (slash == nullptr) ? path : (slash + 1);
  bool hashed = valid && (strnlen(name, kMaxStringScan) > kCardHashLength);
  for (uint8_t i = 0U; hashed && (i < kCardHashLength); ++i) {
    hashed = hex_digit(name[i]);
  }
  return hashed && (name[kCardHashLength] == '.');
}

inline bool gzip_sibling(const char* const path, char (&out)[kCardPathSize]) {
  const bool valid = check(path != nullptr) && check(path[0] == '/');
  const size_t length = valid ? strnlen(path, kCardPathSize) : kCardPathSize;
  const bool fits = (length + 3U) < static_cast<size_t>(kCardPathSize);
  if (fits) {
    static_cast<void>(memcpy(&out[0], path, length));
    static_cast<void>(memcpy(&out[length], ".gz", 4U));
  }
  return fits;
}

template <uint16_t kReadMode, typename SdT>
auto open_card_file(SdT& card, const char* const path, const bool wanted)
    -> decltype(card.open(path, kReadMode)) {
  using FileT = decltype(card.open(path, kReadMode));
  const bool named = check(path != nullptr) && check((!wanted) || (path[0] == '/'));
  FileT file = (named && wanted) ? card.open(path, kReadMode) : FileT();
  const bool folder = static_cast<bool>(file) && file.isDirectory();
  if (folder) {
    file.close();
    return FileT();
  }
  return file;
}

template <typename ExchangeT>
bool send_card_head(ExchangeT& exchange, const char* const path, const uint32_t size,
                    const bool gzip) {
  const bool valid =
      check(path != nullptr) && check(path[0] == '/') && check(!exchange.status_sent());
  const bool status = valid && exchange.send_status(http::kOk);
  const bool typed = status && exchange.send_content_type(content_type_for(path));
  const bool sized = typed && exchange.send_content_length(size);
  const bool encoded =
      sized && ((!gzip) || exchange.send_header(F("Content-Encoding"), FlashText(F("gzip"))));
  const bool varied = encoded && exchange.send_header(F("Vary"), FlashText(F("Accept-Encoding")));
  const bool cached =
      varied && exchange.send_header(
                    F("Cache-Control"),
                    FlashText(hashed_card_name(path) ? F("public, max-age=31536000, immutable")
                                                     : F("no-cache")));
  return cached && exchange.end_headers();
}

}

struct CardPaths {
  const char* plain;
  const char* packed;
};

namespace detail {

template <uint16_t kReadMode, typename ExchangeT, typename SdT>
bool send_card_choice(ExchangeT& exchange, SdT& card, const CardPaths& paths) {
  const char* const path = paths.plain;
  const bool valid = check(path != nullptr) && check(path[0] == '/') &&
                     check((paths.packed == nullptr) || (paths.packed[0] == '/'));
  const bool wants_gzip =
      valid && (paths.packed != nullptr) && accepts_gzip(exchange.header(F("Accept-Encoding")));
  auto file = open_card_file<kReadMode>(card, paths.packed, wants_gzip);
  const bool gzip = static_cast<bool>(file);
  if (!gzip) {
    file = open_card_file<kReadMode>(card, path, valid);
  }
  if (!static_cast<bool>(file)) {
    return exchange.send_empty(http::kNotFound);
  }
  const FileLength length = file_length(file);
  if (!length.fits) {
    file.close();
    return exchange.send_empty(http::kInternalServerError);
  }
  const uint32_t size = length.bytes;
  const bool head = send_card_head(exchange, path, size, gzip);
  const bool body =
      head && ((exchange.method() == Method::kHead) || stream_file(exchange, file, size));
  file.close();
  return body;
}

}

template <uint16_t kReadMode, typename ExchangeT, typename SdT>
bool serve_card_file(ExchangeT& exchange, SdT& card) {
  char path[kCardPathSize] = {};
  char packed[kCardPathSize] = {};
  const bool fresh = check(exchange.path() != nullptr) && check(!exchange.status_sent());
  const bool resolved =
      fresh && resolve_static_path(exchange.path(), "", F("index.html"), &path[0], sizeof(path));
  if (!resolved) {
    return exchange.send_empty(http::kBadRequest);
  }
  const bool has_packed = detail::gzip_sibling(&path[0], packed);
  const CardPaths paths = {&path[0], has_packed ? &packed[0] : nullptr};
  return detail::send_card_choice<kReadMode>(exchange, card, paths);
}

template <uint16_t kReadMode, typename ExchangeT, typename SdT>
bool serve_card_path(ExchangeT& exchange, SdT& card, const char* const flash_path) {
  char path[kCardPathSize] = {};
  char packed[kCardPathSize] = {};
  const bool fits = check(flash_path != nullptr) &&
                    check((strlen_P(flash_path) + 3U) < static_cast<size_t>(kCardPathSize));
  if (!fits) {
    return exchange.send_empty(http::kInternalServerError);
  }
  static_cast<void>(strncpy_P(&path[0], flash_path, sizeof(path) - 1U));
  const bool has_packed = detail::gzip_sibling(&path[0], packed);
  const CardPaths paths = {&path[0], has_packed ? &packed[0] : nullptr};
  return detail::send_card_choice<kReadMode>(exchange, card, paths);
}

}

#endif
