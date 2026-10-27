// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_SEND_ASSET_H
#define MADO_SEND_ASSET_H

#include <Arduino.h>
#include <stdint.h>

#include "Asset.h"
#include "FlashText.h"
#include "Method.h"
#include "accepts_gzip.h"
#include "check.h"
#include "etag_matches.h"
#include "http_status.h"

namespace mado {

constexpr uint8_t kAssetBlockSize = 64U;

namespace detail {

template <typename ExchangeT>
bool send_asset_body(ExchangeT& exchange, const Asset& asset) {
  const bool valid = check(asset.data != nullptr) && check(asset.size > 0U);
  if ((!valid) || (exchange.method() == Method::kHead)) {
    return valid;
  }
  uint8_t block[kAssetBlockSize] = {};
  bool complete = true;
  for (uint16_t sent = 0U; complete && (sent < asset.size);) {
    const uint16_t left = static_cast<uint16_t>(asset.size - sent);
    const uint16_t count = (left < kAssetBlockSize) ? left : kAssetBlockSize;
    static_cast<void>(memcpy_P(&block[0], asset.data + sent, count));
    complete = exchange.write(&block[0], count) == count;
    sent = static_cast<uint16_t>(sent + count);
  }
  return complete;
}

template <typename ExchangeT>
bool send_asset_head(ExchangeT& exchange, const uint16_t code,
                     const __FlashStringHelper* const head) {
  const bool valid = check(head != nullptr) && check(!exchange.status_sent());
  const bool status = valid && exchange.send_status(code);
  const bool written = status && (exchange.print(head) > 0U);
  return written && exchange.end_headers();
}

template <typename ExchangeT>
bool send_not_acceptable(ExchangeT& exchange) {
  const bool fresh = check(!exchange.status_sent());
  const bool status = fresh && exchange.send_status(http::kNotAcceptable);
  const bool vary = status && exchange.send_header(F("Vary"), FlashText(F("Accept-Encoding")));
  const bool length = vary && exchange.send_content_length(0U);
  return length && exchange.end_headers();
}

}

template <typename ExchangeT>
bool send_asset(ExchangeT& exchange, const Asset& asset) {
  const bool valid = check(asset.data != nullptr) && check(asset.size > 0U) &&
                     check(asset.head != nullptr) && check(asset.revalidated != nullptr) &&
                     check(asset.etag != nullptr);
  if ((!valid) || exchange.status_sent()) {
    return false;
  }
  if (etag_matches(exchange.header(F("If-None-Match")), asset.etag)) {
    return detail::send_asset_head(exchange, http::kNotModified, asset.revalidated);
  }
  if (!accepts_gzip(exchange.header(F("Accept-Encoding")))) {
    return detail::send_not_acceptable(exchange);
  }
  const bool ended = detail::send_asset_head(exchange, http::kOk, asset.head);
  return ended && detail::send_asset_body(exchange, asset);
}

}

#endif
