// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_EXCHANGE_H
#define MADO_EXCHANGE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "DecimalText.h"
#include "FlashText.h"
#include "HeaderPool.h"
#include "HeaderSummary.h"
#include "Method.h"
#include "OutputBuffer.h"
#include "ReadStatus.h"
#include "RequestLine.h"
#include "check.h"
#include "equals_ignore_case_P.h"
#include "find_query_param.h"
#include "flash_address.h"
#include "http_status.h"
#include "kMaxStringScan.h"
#include "name_at.h"
#include "read_byte.h"
#include "reason_phrase.h"
#include "wait_for_data.h"

namespace mado {

template <typename ClientT, typename LimitsT>
class Exchange {
 public:
  using Limits = LimitsT;

  Exchange(ClientT& client, OutputBuffer<ClientT, LimitsT>& out, const HeaderPool<LimitsT>& pool,
           const char* const* const names, const RequestLine& line, const HeaderSummary& headers,
           const uint32_t started)
      : client_(&client),
        out_(&out),
        pool_(&pool),
        names_(names),
        line_(line),
        content_length_(headers.content_length),
        remaining_(headers.content_length),
        expect_continue_(headers.expect_continue),
        started_(started) {
    const bool has_path = check(line.path != nullptr);
    const bool has_query = check(line.query != nullptr);
    if (!has_path) {
      line_.path = "";
    }
    if (!has_query) {
      line_.query = "";
    }
  }

  Method method() const {
    const bool sound = check(line_.path != nullptr) && check(remaining_ <= content_length_);
    return sound ? line_.method : Method::kUnknown;
  }

  const char* path() const {
    const bool has_path = check(line_.path != nullptr);
    const bool rooted = has_path && check(line_.path[0] != '\0');
    return rooted ? line_.path : "/";
  }

  const char* query() const {
    const bool has_query = check(line_.query != nullptr);
    const bool sound = check(remaining_ <= content_length_);
    return (has_query && sound) ? line_.query : "";
  }

  uint32_t content_length() const {
    const bool sound = check(remaining_ <= content_length_);
    const bool consistent = check(expect_continue_ || (!continue_sent_));
    return (sound && consistent) ? content_length_ : 0UL;
  }

  uint32_t remaining_body() const {
    const bool sound = check(remaining_ <= content_length_);
    const bool consistent = check(expect_continue_ || (!continue_sent_));
    return (sound && consistent) ? remaining_ : 0UL;
  }

  uint32_t client_address() const {
    const bool has_client = check(client_ != nullptr);
    const uint32_t address = has_client ? static_cast<uint32_t>(client_->remoteIP()) : 0U;
    return check(address != 0U) ? address : 0U;
  }

  ClientT* client() {
    const bool has_client = check(client_ != nullptr);
    const bool has_output = check(out_ != nullptr);
    return (has_client && has_output) ? client_ : nullptr;
  }

  const char* header(const char* const name) const {
    const bool valid =
        check(name != nullptr) && check(names_ != nullptr) && check(pool_ != nullptr);
    if (!valid) {
      return nullptr;
    }
    const char* found = nullptr;
    for (uint8_t i = 0U; i < LimitsT::kMaxCapturedHeaders; ++i) {
      const char* const candidate = name_at(names_, i);
      if (candidate == nullptr) {
        break;
      }
      if (equals_ignore_case_P(name, candidate)) {
        found = pool_->value(i);
        break;
      }
    }
    return found;
  }

  const char* header(const __FlashStringHelper* const name) const {
    const bool valid = check(name != nullptr) && check(LimitsT::kHeaderNameSize > 1U);
    char copy[LimitsT::kHeaderNameSize] = {};
    if (valid) {
      static_cast<void>(strncpy_P(&copy[0], flash_address(name), sizeof(copy) - 1U));
    }
    return valid ? header(&copy[0]) : nullptr;
  }

  bool query_param(const char* const name, char* const out, const uint16_t size) const {
    const bool valid = check(name != nullptr) && check(out != nullptr) && check(size > 0U);
    if (!valid) {
      return false;
    }
    return find_query_param(query(), name, out, size);
  }

  bool query_param(const __FlashStringHelper* const name, char* const out,
                   const uint16_t size) const {
    const bool valid = check(name != nullptr) && check(out != nullptr);
    char copy[kMaxQueryName] = {};
    if (valid) {
      static_cast<void>(strncpy_P(&copy[0], flash_address(name), sizeof(copy) - 1U));
    }
    return valid && query_param(&copy[0], out, size);
  }

  int16_t read_body(uint8_t* const buffer, const uint16_t size) {
    const bool valid = check(buffer != nullptr) && check(size > 0U) && check(size <= kMaxBodyChunk);
    const bool sound = check(client_ != nullptr) && check(remaining_ <= content_length_);
    if (!(valid && sound)) {
      return -1;
    }
    if (remaining_ == 0UL) {
      return 0;
    }
    send_continue_once();
    int32_t available = 0;
    const ReadStatus waited = wait_for_data<LimitsT>(*client_, available, started_);
    if (waited != ReadStatus::kByte) {
      return -1;
    }
    const uint32_t wanted = smallest(smallest(remaining_, size), static_cast<uint32_t>(available));
    const int32_t got = client_->read(buffer, static_cast<size_t>(wanted));
    const bool read_ok = check(got > 0) && check(static_cast<uint32_t>(got) <= wanted);
    if (!read_ok) {
      return -1;
    }
    remaining_ -= static_cast<uint32_t>(got);
    return static_cast<int16_t>(got);
  }

  bool discard_body() {
    const bool sound = check(client_ != nullptr) && check(remaining_ <= content_length_);
    const bool withheld = expect_continue_ && (!continue_sent_);
    if ((!sound) || withheld || (remaining_ > LimitsT::kMaxDrainBytes)) {
      return remaining_ == 0UL;
    }
    for (uint16_t i = 0U; (i < LimitsT::kMaxDrainBytes) && (remaining_ > 0UL); ++i) {
      uint8_t scrap = 0U;
      const ReadStatus status = read_byte<LimitsT>(*client_, scrap, started_);
      if (status != ReadStatus::kByte) {
        break;
      }
      --remaining_;
    }
    const bool sound_after = check(remaining_ <= content_length_);
    return sound_after && (remaining_ == 0UL);
  }

  bool send_status(const uint16_t code) {
    const bool valid = check(code >= http::kFirstValid) && check(code <= http::kLastValid);
    const bool fresh = check(!headers_ended_) && (!status_sent_);
    if (!(valid && fresh)) {
      return false;
    }
    status_sent_ = true;
    status_code_ = code;
    size_t written = print(F("HTTP/1.1 "));
    written += print(static_cast<uint32_t>(code));
    written += write_byte(static_cast<uint8_t>(' '));
    written += out_->write_flash(reason_phrase(code));
    written += print(F("\r\nConnection: close\r\n"));
    return written > 0U;
  }

  bool send_header(const __FlashStringHelper* const name, const char* const value) {
    const bool has_value = check(value != nullptr);
    if (!has_value) {
      return false;
    }
    const bool ready = begin_header(name);
    if (!ready) {
      return false;
    }
    const size_t value_length = print(value);
    const size_t end_length = print(F("\r\n"));
    return check(end_length > 0U) && check((value_length > 0U) || (value[0] == '\0'));
  }

  bool send_header(const __FlashStringHelper* const name, const FlashText& flash_value) {
    const __FlashStringHelper* const value = flash_value.get();
    const bool has_value = check(value != nullptr);
    if (!has_value) {
      return false;
    }
    const bool ready = begin_header(name);
    if (!ready) {
      return false;
    }
    const size_t value_length = print(value);
    const size_t end_length = print(F("\r\n"));
    return check(end_length > 0U) && check(value_length > 0U);
  }

  bool send_header(const __FlashStringHelper* const name, const uint32_t value) {
    const bool ready = begin_header(name);
    if (!ready) {
      return false;
    }
    const size_t value_length = print(value);
    const size_t end_length = print(F("\r\n"));
    return check(end_length > 0U) && check(value_length > 0U);
  }

  bool send_location(const __FlashStringHelper* const prefix, const uint32_t resource_id) {
    const bool valid = check(prefix != nullptr) && check(status_sent_);
    const bool ready = valid && begin_header(F("Location"));
    if (!ready) {
      return false;
    }
    const size_t prefix_length = print(prefix);
    const size_t id_length = print(resource_id);
    const size_t end_length = print(F("\r\n"));
    return check(end_length > 0U) && (prefix_length > 0U) && (id_length > 0U);
  }

  bool send_content_type(const __FlashStringHelper* const type) {
    const bool valid = check(type != nullptr) && check(status_sent_);
    if (!valid) {
      return false;
    }
    return send_header(F("Content-Type"), FlashText(type));
  }

  bool send_content_length(const uint32_t length) {
    const bool ready = check(status_sent_) && check(!headers_ended_);
    if (!ready) {
      return false;
    }
    return send_header(F("Content-Length"), length);
  }

  bool send_cors_headers() {
    const bool ready = check(status_sent_) && check(!headers_ended_);
    if (!ready) {
      return false;
    }
    const bool origin = send_header(F("Access-Control-Allow-Origin"), FlashText(F("*")));
    const bool methods = send_header(F("Access-Control-Allow-Methods"),
                                     FlashText(F("GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS")));
    const bool headers =
        send_header(F("Access-Control-Allow-Headers"), FlashText(F("Content-Type, Authorization")));
    return origin && methods && headers;
  }

  bool end_headers() {
    const bool ready = check(status_sent_) && check(!headers_ended_);
    if (!ready) {
      return false;
    }
    const size_t written = print(F("\r\n"));
    headers_ended_ = true;
    out_->set_discard(line_.method == Method::kHead);
    return written > 0U;
  }

  bool send_empty(const uint16_t code) {
    const bool valid = check(code >= http::kFirstValid) && check(code <= http::kLastValid);
    if (!valid) {
      return false;
    }
    const bool sent = send_status(code);
    if (!sent) {
      return false;
    }
    const bool length = send_content_length(0UL);
    const bool ended = end_headers();
    return length && ended;
  }

  bool send_redirect(const __FlashStringHelper* const location) {
    const bool valid = check(location != nullptr) && check(!headers_ended_);
    if (!valid) {
      return false;
    }
    const bool sent = send_status(http::kSeeOther);
    if (!sent) {
      return false;
    }
    const bool located = send_header(F("Location"), FlashText(location));
    const bool length = send_content_length(0UL);
    const bool ended = end_headers();
    return located && length && ended;
  }

  size_t write(const uint8_t value) {
    const bool has_output = check(out_ != nullptr);
    const bool sound = check(remaining_ <= content_length_);
    return (has_output && sound) ? write_byte(value) : 0U;
  }

  size_t write(const uint8_t* const data, const size_t length) {
    const bool valid = check(data != nullptr) || check(length == 0U);
    const bool has_output = check(out_ != nullptr);
    return (valid && has_output) ? out_->write(data, length) : 0U;
  }

  size_t print(const char* const text) {
    const bool valid = check(text != nullptr) && check(out_ != nullptr);
    if (!valid) {
      return 0U;
    }
    size_t written = 0U;
    for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
      const char symbol = text[i];
      if (symbol == '\0') {
        break;
      }
      written += write_byte(static_cast<uint8_t>(symbol));
    }
    return written;
  }

  size_t print(const __FlashStringHelper* const text) {
    const bool valid = check(text != nullptr);
    const bool has_output = check(out_ != nullptr);
    return (valid && has_output) ? out_->write_flash(flash_address(text)) : 0U;
  }

  size_t print(const uint32_t value) {
    const DecimalText text(value);
    const uint8_t length = text.length();
    const bool sound = check(length > 0U) && check(out_ != nullptr);
    if (!sound) {
      return 0U;
    }
    const char* const digits = text.begin();
    size_t written = 0U;
    for (uint8_t i = 0U; i < length; ++i) {
      written += write_byte(static_cast<uint8_t>(digits[i]));
    }
    return written;
  }

  size_t print_reason(const uint16_t code) {
    const bool valid = check(code >= http::kFirstValid) && check(code <= http::kLastValid);
    const bool has_output = check(out_ != nullptr);
    return (valid && has_output) ? out_->write_flash(reason_phrase(code)) : 0U;
  }

  size_t print_signed(const int32_t value) {
    const bool negative = value < 0;
    const bool has_output = check(out_ != nullptr);
    if (!has_output) {
      return 0U;
    }
    size_t written = 0U;
    if (negative) {
      written += write_byte(static_cast<uint8_t>('-'));
    }
    const uint32_t magnitude =
        negative ? (static_cast<uint32_t>(-(value + 1)) + 1UL) : static_cast<uint32_t>(value);
    const bool in_range = check(magnitude <= kLargestMagnitude);
    return in_range ? (written + print(magnitude)) : written;
  }

  uint16_t status_code() const {
    const bool consistent = check(status_sent_ == (status_code_ != http::kNoResponse));
    const bool in_range = check(status_code_ <= http::kLastValid);
    return (consistent && in_range) ? status_code_ : http::kNoResponse;
  }

  bool status_sent() const {
    const bool consistent = check(status_sent_ || (!headers_ended_));
    const bool continued = check(expect_continue_ || (!continue_sent_));
    return status_sent_ || (!consistent) || (!continued);
  }

 private:
  static constexpr uint16_t kMaxBodyChunk = 0x7FFFU;
  static constexpr uint8_t kMaxQueryName = 32U;
  static constexpr uint32_t kLargestMagnitude = 0x80000000UL;

  static uint32_t smallest(const uint32_t first, const uint32_t second) {
    return (first < second) ? first : second;
  }

  size_t write_byte(const uint8_t value) {
    const bool has_output = check(out_ != nullptr);
    const bool sound = check(remaining_ <= content_length_);
    return (has_output && sound) ? out_->write_byte(value) : 0U;
  }

  bool begin_header(const __FlashStringHelper* const name) {
    const bool ready = check(name != nullptr) && check(status_sent_) && check(!headers_ended_);
    if (!ready) {
      return false;
    }
    const size_t name_length = print(name);
    const size_t separator_length = print(F(": "));
    return (name_length > 0U) && (separator_length > 0U);
  }

  void send_continue_once() {
    const bool needed = expect_continue_ && (!continue_sent_) && (!status_sent_);
    const bool has_output = check(out_ != nullptr);
    if ((!needed) || (!has_output)) {
      return;
    }
    const size_t written = print(F("HTTP/1.1 100 Continue\r\n\r\n"));
    const bool flushed = out_->flush();
    continue_sent_ = check(written > 0U) && check(flushed);
  }

  ClientT* client_;
  OutputBuffer<ClientT, LimitsT>* out_;
  const HeaderPool<LimitsT>* pool_;
  const char* const* names_;
  RequestLine line_;
  uint32_t content_length_;
  uint32_t remaining_;
  bool expect_continue_;
  uint32_t started_;
  uint16_t status_code_ = http::kNoResponse;
  bool continue_sent_ = false;
  bool status_sent_ = false;
  bool headers_ended_ = false;
};

}

#endif
