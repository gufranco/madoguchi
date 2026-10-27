// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEADER_PARSER_H
#define MADO_HEADER_PARSER_H

#include <Arduino.h>
#include <stdint.h>

#include "DefaultPolicy.h"
#include "HeaderKind.h"
#include "HeaderPool.h"
#include "HeaderStatus.h"
#include "HeaderSummary.h"
#include "HostMatcher.h"
#include "ReadStatus.h"
#include "check.h"
#include "equals_ignore_case_P.h"
#include "kMaxStringScan.h"
#include "name_at.h"
#include "read_byte.h"
#include "to_lower_ascii.h"

namespace mado {

template <typename LimitsT, typename ClientT, typename PolicyT = DefaultPolicy>
class HeaderParser {
 public:
  static_assert(LimitsT::kHeaderNameSize > 1U, "a header name needs room");
  static_assert(LimitsT::kHeaderNameSize <= 0xFFU, "name lengths are 8-bit");
  static_assert(LimitsT::kMaxHeaderLines > 0U, "at least one header line must be allowed");

  HeaderParser(ClientT& client, const char* const* const names, HeaderPool<LimitsT>& pool,
               const uint32_t request_started)
      : client_(&client), names_(names), pool_(&pool), request_started_(request_started) {
  }

  HeaderSummary parse() {
    const bool wired = check(client_ != nullptr) && check(pool_ != nullptr);
    if (!wired) {
      summary_.status = HeaderStatus::kMalformed;
      return summary_;
    }
    summary_.host_trusted = !PolicyT::kTrustedHostsOnly;
    for (uint8_t line = 0U; line < LimitsT::kMaxHeaderLines; ++line) {
      const LineOutcome outcome = read_name();
      if (outcome == LineOutcome::kEnd) {
        return summary_;
      }
      if (outcome != LineOutcome::kName) {
        summary_.status = to_status(outcome);
        return summary_;
      }
      const HeaderStatus classified = classify();
      if (classified != HeaderStatus::kOk) {
        summary_.status = classified;
        return summary_;
      }
      const HeaderStatus value_status = read_value();
      if (value_status != HeaderStatus::kOk) {
        summary_.status = value_status;
        return summary_;
      }
    }
    summary_.status = HeaderStatus::kTooMany;
    return summary_;
  }

 private:
  static constexpr uint8_t kExpectLength = 12U;
  static constexpr uint32_t kDecimalBase = 10UL;
  static constexpr uint32_t kMaxLengthPrefix = 429496729UL;
  static constexpr uint32_t kMaxLengthLastDigit = 5UL;

  enum class LineOutcome : uint8_t { kName, kEnd, kMalformed, kTimeout, kClosed };

  static HeaderStatus to_status(const LineOutcome outcome) {
    const bool failure =
        check(outcome != LineOutcome::kName) && check(outcome != LineOutcome::kEnd);
    HeaderStatus status = HeaderStatus::kMalformed;
    if (failure && (outcome == LineOutcome::kTimeout)) {
      status = HeaderStatus::kTimeout;
    }
    if (failure && (outcome == LineOutcome::kClosed)) {
      status = HeaderStatus::kClosed;
    }
    return status;
  }

  static LineOutcome from_read(const ReadStatus status) {
    const bool failure = check(status != ReadStatus::kByte);
    LineOutcome outcome = LineOutcome::kMalformed;
    if (failure) {
      outcome = (status == ReadStatus::kTimeout) ? LineOutcome::kTimeout : LineOutcome::kClosed;
    }
    return outcome;
  }

  static bool is_token_char(const char symbol) {
    const bool alpha = ((symbol >= 'a') && (symbol <= 'z')) || ((symbol >= 'A') && (symbol <= 'Z'));
    const bool digit = (symbol >= '0') && (symbol <= '9');
    return alpha || digit || (symbol == '-') || (symbol == '_');
  }

  LineOutcome read_name() {
    name_length_ = 0U;
    name_overflow_ = false;
    seen_return_ = false;
    const bool wired = check(client_ != nullptr);
    if (!wired) {
      return LineOutcome::kMalformed;
    }
    for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
      uint8_t value = 0U;
      const ReadStatus status = read_byte<LimitsT>(*client_, value, request_started_);
      if (status != ReadStatus::kByte) {
        return from_read(status);
      }
      const char symbol = static_cast<char>(value);
      const LineOutcome outcome = accept_name_char(symbol);
      const bool bounded = check(name_length_ < LimitsT::kHeaderNameSize);
      if (!bounded) {
        return LineOutcome::kMalformed;
      }
      if (outcome != LineOutcome::kName) {
        return outcome;
      }
      if (symbol == ':') {
        return LineOutcome::kName;
      }
    }
    return LineOutcome::kMalformed;
  }

  LineOutcome accept_name_char(const char symbol) {
    const bool bounded = check(name_length_ < LimitsT::kHeaderNameSize);
    const bool overflow_consistent =
        check((!name_overflow_) || ((name_length_ + 1U) == LimitsT::kHeaderNameSize));
    if (!(bounded && overflow_consistent)) {
      return LineOutcome::kMalformed;
    }
    const bool empty = name_length_ == 0U;
    if (symbol == '\r') {
      seen_return_ = true;
      return empty ? LineOutcome::kName : LineOutcome::kMalformed;
    }
    if (symbol == '\n') {
      return empty ? LineOutcome::kEnd : LineOutcome::kMalformed;
    }
    if (symbol == ':') {
      name_[name_length_] = '\0';
      return empty ? LineOutcome::kMalformed : LineOutcome::kName;
    }
    const bool valid = is_token_char(symbol) && (!seen_return_);
    if (!valid) {
      return LineOutcome::kMalformed;
    }
    const bool has_room = (static_cast<uint16_t>(name_length_) + 1U) < LimitsT::kHeaderNameSize;
    if (has_room) {
      name_[name_length_] = symbol;
      ++name_length_;
    }
    name_overflow_ = name_overflow_ || (!has_room);
    return LineOutcome::kName;
  }

  HeaderStatus classify() {
    kind_ = HeaderKind::kIgnored;
    slot_ = kNoSlot;
    is_host_ = false;
    const bool bounded = check(name_length_ < LimitsT::kHeaderNameSize);
    const bool terminated = bounded && check(name_[name_length_] == '\0');
    if (name_overflow_ || (!terminated)) {
      return HeaderStatus::kOk;
    }
    const HeaderStatus special = classify_framing();
    if ((special != HeaderStatus::kOk) || (kind_ != HeaderKind::kIgnored)) {
      return special;
    }
    slot_ = find_slot();
    if (slot_ != kNoSlot) {
      kind_ = HeaderKind::kCaptured;
    }
    return HeaderStatus::kOk;
  }

  HeaderStatus classify_framing() {
    const bool fresh = check(kind_ == HeaderKind::kIgnored) && check(!is_host_);
    if (!fresh) {
      return HeaderStatus::kMalformed;
    }
    if (equals_ignore_case_P(&name_[0], PSTR("Content-Length"))) {
      kind_ = HeaderKind::kContentLength;
      return summary_.has_content_length ? HeaderStatus::kMalformed : HeaderStatus::kOk;
    }
    if (equals_ignore_case_P(&name_[0], PSTR("Expect"))) {
      kind_ = HeaderKind::kExpect;
      return HeaderStatus::kOk;
    }
    if (equals_ignore_case_P(&name_[0], PSTR("Transfer-Encoding"))) {
      return HeaderStatus::kUnsupportedCoding;
    }
    if (equals_ignore_case_P(&name_[0], PSTR("Host"))) {
      is_host_ = true;
      const bool repeated = summary_.has_host;
      summary_.has_host = true;
      return repeated ? HeaderStatus::kMalformed : HeaderStatus::kOk;
    }
    return HeaderStatus::kOk;
  }

  uint8_t find_slot() const {
    const bool has_names = names_ != nullptr;
    const bool bounded = check(name_length_ < LimitsT::kHeaderNameSize);
    if ((!has_names) || (!bounded)) {
      return kNoSlot;
    }
    uint8_t found = kNoSlot;
    for (uint8_t i = 0U; i < LimitsT::kMaxCapturedHeaders; ++i) {
      const char* const candidate = name_at(names_, i);
      if (candidate == nullptr) {
        break;
      }
      if (equals_ignore_case_P(&name_[0], candidate)) {
        found = i;
        break;
      }
    }
    return found;
  }

  HeaderStatus read_value() {
    const bool wired = check(client_ != nullptr);
    const bool slotted =
        check((kind_ != HeaderKind::kCaptured) || (slot_ < LimitsT::kMaxCapturedHeaders));
    if (!(wired && slotted)) {
      return HeaderStatus::kMalformed;
    }
    start_value();
    for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
      uint8_t value = 0U;
      const ReadStatus status = read_byte<LimitsT>(*client_, value, request_started_);
      if (status != ReadStatus::kByte) {
        return to_status(from_read(status));
      }
      const char symbol = static_cast<char>(value);
      if (symbol == '\n') {
        return finish_value();
      }
      const bool accepted = accept_value_char(symbol);
      if (!accepted) {
        return HeaderStatus::kMalformed;
      }
    }
    return HeaderStatus::kMalformed;
  }

  void start_value() {
    value_started_ = false;
    value_ended_ = false;
    expect_index_ = 0U;
    expect_mismatch_ = false;
    const bool wired = check(pool_ != nullptr);
    const bool slotted = check((kind_ != HeaderKind::kCaptured) || (slot_ != kNoSlot));
    if (!(wired && slotted)) {
      kind_ = HeaderKind::kIgnored;
    }
    if (kind_ == HeaderKind::kCaptured) {
      const bool opened = pool_->begin(slot_);
      kind_ = opened ? HeaderKind::kCaptured : HeaderKind::kIgnored;
    }
    if (kind_ == HeaderKind::kContentLength) {
      summary_.content_length = 0U;
    }
    if (is_host_) {
      host_.begin();
    }
  }

  bool accept_value_char(const char symbol) {
    const bool expect_bounded = check(expect_index_ <= kExpectLength);
    const bool ordered = check(value_started_ || (!value_ended_));
    if (!(expect_bounded && ordered)) {
      return false;
    }
    const bool blank = (symbol == ' ') || (symbol == '\t') || (symbol == '\r');
    if (blank && (!value_started_)) {
      return true;
    }
    if (blank) {
      value_ended_ = true;
      return true;
    }
    const bool resumed = value_ended_;
    value_started_ = true;
    value_ended_ = false;
    if (is_host_ && resumed) {
      return false;
    }
    if (is_host_) {
      host_.accept(symbol);
    }
    if (kind_ == HeaderKind::kContentLength) {
      if (resumed) {
        return false;
      }
      return accept_length_digit(symbol);
    }
    if (kind_ == HeaderKind::kExpect) {
      accept_expect_char(symbol, resumed);
    }
    if (kind_ == HeaderKind::kCaptured) {
      append_captured(symbol, resumed);
    }
    return true;
  }

  bool accept_length_digit(const char symbol) {
    const bool length_header = check(kind_ == HeaderKind::kContentLength);
    const bool started = check(value_started_);
    const bool digit = (symbol >= '0') && (symbol <= '9');
    if (!(length_header && started && digit)) {
      return false;
    }
    const uint32_t digit_value = static_cast<uint32_t>(symbol - '0');
    const uint32_t current = summary_.content_length;
    const bool below = current < kMaxLengthPrefix;
    const bool at_edge = (current == kMaxLengthPrefix) && (digit_value <= kMaxLengthLastDigit);
    if (!(below || at_edge)) {
      return false;
    }
    summary_.content_length = (current * kDecimalBase) + digit_value;
    summary_.has_content_length = true;
    return true;
  }

  void accept_expect_char(const char symbol, const bool resumed) {
    const bool expect_header = check(kind_ == HeaderKind::kExpect);
    const bool in_range = check(expect_index_ < kExpectLength);
    if ((!expect_header) || (!in_range) || resumed) {
      expect_mismatch_ = true;
      return;
    }
    const char* const token = PSTR("100-continue");
    const char expected = static_cast<char>(pgm_read_byte(token + expect_index_));
    const bool same = to_lower_ascii(symbol) == expected;
    expect_mismatch_ = expect_mismatch_ || (!same);
    ++expect_index_;
  }

  void append_captured(const char symbol, const bool resumed) {
    const bool captured = check(kind_ == HeaderKind::kCaptured);
    const bool wired = check(pool_ != nullptr);
    if (!(captured && wired)) {
      return;
    }
    if (resumed) {
      pool_->append(' ');
    }
    pool_->append(symbol);
  }

  HeaderStatus finish_value() {
    const bool expect_bounded = check(expect_index_ <= kExpectLength);
    const bool wired = check(pool_ != nullptr);
    if (!(expect_bounded && wired)) {
      return HeaderStatus::kMalformed;
    }
    if (kind_ == HeaderKind::kCaptured) {
      pool_->finish();
    }
    const bool counted = check((!is_host_) || summary_.has_host);
    if (is_host_ && counted) {
      summary_.host_trusted = host_.trusted();
    }
    if (kind_ == HeaderKind::kExpect) {
      const bool complete = expect_index_ == kExpectLength;
      summary_.expect_continue = complete && (!expect_mismatch_);
    }
    const bool length_missing = (kind_ == HeaderKind::kContentLength) && (!value_started_);
    return length_missing ? HeaderStatus::kMalformed : HeaderStatus::kOk;
  }

  ClientT* client_;
  const char* const* names_;
  HeaderPool<LimitsT>* pool_;
  uint32_t request_started_;
  HeaderSummary summary_ = {HeaderStatus::kOk, 0U, false, false, false, false};
  HostMatcher<PolicyT> host_;
  bool is_host_ = false;
  char name_[LimitsT::kHeaderNameSize] = {};
  uint8_t name_length_ = 0U;
  bool name_overflow_ = false;
  bool seen_return_ = false;
  HeaderKind kind_ = HeaderKind::kIgnored;
  uint8_t slot_ = kNoSlot;
  bool value_started_ = false;
  bool value_ended_ = false;
  uint8_t expect_index_ = 0U;
  bool expect_mismatch_ = false;
};

}

#endif
