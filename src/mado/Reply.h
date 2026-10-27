// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_REPLY_H
#define MADO_REPLY_H

#include <Arduino.h>
#include <stdint.h>

#include "FlashText.h"
#include "check.h"
#include "http_status.h"

namespace mado {

template <typename ExchangeT>
class Reply {
 public:
  explicit Reply(ExchangeT& exchange) : exchange_(&exchange), ok_(check(!exchange.status_sent())) {
  }

  Reply& status(const uint16_t code) {
    const bool valid = check(code >= http::kFirstValid) && check(code <= http::kLastValid);
    ok_ = ok_ && valid && exchange_->send_status(code);
    return *this;
  }

  Reply& type(const __FlashStringHelper* const media_type) {
    const bool valid = check(media_type != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid && exchange_->send_content_type(media_type);
    return *this;
  }

  Reply& json() {
    const bool valid = check(exchange_ != nullptr) && check(exchange_->status_sent() || (!ok_));
    ok_ = ok_ && valid && exchange_->send_content_type(F("application/json"));
    ok_ = ok_ && exchange_->send_header(F("X-Content-Type-Options"), FlashText(F("nosniff")));
    return *this;
  }

  Reply& no_store() {
    const bool valid = check(exchange_ != nullptr);
    ok_ = ok_ && valid;
    return header(F("Cache-Control"), FlashText(F("no-store")));
  }

  Reply& header(const __FlashStringHelper* const name, const FlashText& value) {
    const bool valid = check(name != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid && exchange_->send_header(name, value);
    return *this;
  }

  Reply& header(const __FlashStringHelper* const name, const char* const value) {
    const bool valid = check(name != nullptr) && check(value != nullptr);
    ok_ = ok_ && valid && exchange_->send_header(name, value);
    return *this;
  }

  Reply& header(const __FlashStringHelper* const name, const uint32_t value) {
    const bool valid = check(name != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid && exchange_->send_header(name, value);
    return *this;
  }

  Reply& location(const __FlashStringHelper* const prefix, const uint32_t resource_id) {
    const bool valid = check(prefix != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid && exchange_->send_location(prefix, resource_id);
    return *this;
  }

  Reply& length(const uint32_t bytes) {
    const bool valid = check(exchange_ != nullptr) && check(exchange_->status_sent() || (!ok_));
    ok_ = ok_ && valid && exchange_->send_content_length(bytes);
    return *this;
  }

  bool end() {
    const bool valid = check(exchange_ != nullptr) && check(exchange_->status_sent() || (!ok_));
    ok_ = ok_ && valid && exchange_->end_headers();
    return ok_;
  }

 private:
  ExchangeT* exchange_;
  bool ok_;
};

}

#endif
