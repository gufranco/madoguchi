// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_TOKEN_WRITER_H
#define MADO_TOKEN_WRITER_H

#include <Arduino.h>
#include <stdint.h>

#include "Base64UrlEncoder.h"
#include "DecimalText.h"
#include "HmacSha256.h"
#include "KeyMemory.h"
#include "check.h"
#include "kMaxStringScan.h"
#include "kSha256Size.h"
#include "kTokenKeySize.h"

namespace mado {

template <typename ExchangeT>
class TokenWriter {
 public:
  TokenWriter(ExchangeT& exchange, const uint8_t* const flash_key) : exchange_(&exchange) {
    const bool has_key = check(flash_key != nullptr);
    ok_ = has_key && mac_.begin(flash_key, kTokenKeySize, KeyMemory::kFlash);
  }

  void signed_flash(const char* const text) {
    const bool valid = check(text != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid;
    for (uint16_t i = 0U; ok_ && (i < kMaxStringScan); ++i) {
      const char symbol = static_cast<char>(pgm_read_byte(text + i));
      if (symbol == '\0') {
        break;
      }
      signed_char(symbol);
    }
  }

  void signed_char(const char symbol) {
    const bool wired = check(exchange_ != nullptr) && check(symbol != '\0');
    const uint8_t value = static_cast<uint8_t>(symbol);
    const bool written = wired && (exchange_->write(value) == 1U);
    ok_ = ok_ && written && mac_.update(value);
  }

  void encoded_byte(const uint8_t value) {
    char chunk[Base64UrlEncoder::kMaxOutput] = {};
    const uint8_t count = encoder_.push(value, &chunk[0]);
    const bool bounded =
        check(count <= Base64UrlEncoder::kMaxOutput) && check(exchange_ != nullptr);
    ok_ = ok_ && bounded;
    for (uint8_t i = 0U; ok_ && (i < count); ++i) {
      signed_char(chunk[i]);
    }
  }

  void encoded_flash(const char* const text) {
    const bool valid = check(text != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid;
    for (uint16_t i = 0U; ok_ && (i < kMaxStringScan); ++i) {
      const uint8_t value = pgm_read_byte(text + i);
      if (value == 0U) {
        break;
      }
      encoded_byte(value);
    }
  }

  void encoded_ram(const char* const text) {
    const bool valid = check(text != nullptr) && check(exchange_ != nullptr);
    ok_ = ok_ && valid;
    for (uint16_t i = 0U; ok_ && (i < kMaxStringScan); ++i) {
      const uint8_t value = static_cast<uint8_t>(text[i]);
      if (value == 0U) {
        break;
      }
      encoded_byte(value);
    }
  }

  void encoded_number(const uint32_t value) {
    const DecimalText text(value);
    const bool sound = check(text.length() > 0U) && check(text.length() <= kMaxDecimalDigits) &&
                       check(exchange_ != nullptr);
    ok_ = ok_ && sound;
    const char* const digits = text.begin();
    for (uint8_t i = 0U; ok_ && (i < text.length()); ++i) {
      encoded_byte(static_cast<uint8_t>(digits[i]));
    }
  }

  void end_segment() {
    char chunk[Base64UrlEncoder::kMaxOutput] = {};
    const uint8_t count = encoder_.finish(&chunk[0]);
    const bool bounded =
        check(count <= Base64UrlEncoder::kMaxOutput) && check(exchange_ != nullptr);
    ok_ = ok_ && bounded;
    for (uint8_t i = 0U; ok_ && (i < count); ++i) {
      signed_char(chunk[i]);
    }
  }

  bool sign() {
    uint8_t digest[kSha256Size] = {};
    const bool wired = check(exchange_ != nullptr);
    ok_ = ok_ && wired && mac_.finish(&digest[0]);
    ok_ = ok_ && (exchange_->write(static_cast<uint8_t>('.')) == 1U);
    Base64UrlEncoder encoder;
    char chunk[Base64UrlEncoder::kMaxOutput] = {};
    for (uint8_t i = 0U; ok_ && (i <= kSha256Size); ++i) {
      const uint8_t count =
          (i < kSha256Size) ? encoder.push(digest[i], &chunk[0]) : encoder.finish(&chunk[0]);
      ok_ = check(count <= Base64UrlEncoder::kMaxOutput) && unsigned_chars(&chunk[0], count);
    }
    return ok_;
  }

 private:
  static constexpr uint8_t kMaxDecimalDigits = 10U;

  bool unsigned_chars(const char* const chunk, const uint8_t count) {
    const bool valid = check(chunk != nullptr) && check(exchange_ != nullptr);
    bool written = valid;
    for (uint8_t i = 0U; written && (i < count); ++i) {
      written = exchange_->write(static_cast<uint8_t>(chunk[i])) == 1U;
    }
    return written;
  }

  ExchangeT* exchange_;
  HmacSha256 mac_;
  Base64UrlEncoder encoder_;
  bool ok_ = false;
};

}

#endif
