// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_JSON_READER_H
#define MADO_JSON_READER_H

#include <Arduino.h>
#include <stdint.h>

#include "JsonStatus.h"
#include "check.h"
#include "kMaxStringScan.h"

namespace mado {

template <typename SourceT>
class JsonReader {
 public:
  explicit JsonReader(SourceT& source) : source_(&source) {
    const bool wired = check(source_ != nullptr);
    state_ = wired ? State::kStart : State::kFailed;
  }

  JsonStatus begin_object() {
    const bool fresh = check(state_ == State::kStart) && check(source_ != nullptr);
    if (!fresh) {
      return fail(JsonStatus::kSyntax);
    }
    const int16_t symbol = next_significant();
    if (symbol != '{') {
      return fail(missing_or(symbol, JsonStatus::kWrongType));
    }
    state_ = State::kFirstMember;
    return JsonStatus::kOk;
  }

  JsonStatus next_key(char* const key, const uint8_t size) {
    const bool valid = check(key != nullptr) && check(size > 1U) && check(source_ != nullptr);
    const bool placed = (state_ == State::kFirstMember) || (state_ == State::kAfterValue);
    if (!(valid && placed)) {
      return fail(JsonStatus::kSyntax);
    }
    const int16_t symbol = member_start();
    if (symbol == '}') {
      state_ = State::kEnded;
      return JsonStatus::kEnd;
    }
    if (symbol != '"') {
      return fail(missing_or(symbol, JsonStatus::kSyntax));
    }
    const JsonStatus read = read_string_body(key, size);
    if (read != JsonStatus::kOk) {
      return fail(read);
    }
    const int16_t colon = next_significant();
    if (colon != ':') {
      return fail(missing_or(colon, JsonStatus::kSyntax));
    }
    state_ = State::kAfterKey;
    return JsonStatus::kOk;
  }

  JsonStatus read_string(char* const out, const uint8_t size) {
    const bool valid = check(out != nullptr) && check(size > 0U);
    if ((!valid) || (state_ != State::kAfterKey)) {
      return fail(JsonStatus::kSyntax);
    }
    const int16_t symbol = next_significant();
    if (symbol != '"') {
      return fail(missing_or(symbol, JsonStatus::kWrongType));
    }
    const JsonStatus read = read_string_body(out, size);
    state_ = State::kAfterValue;
    return (read == JsonStatus::kOk) ? read : fail(read);
  }

  JsonStatus read_integer(int32_t& out) {
    const bool placed = check(state_ != State::kFailed) && check(source_ != nullptr) &&
                        (state_ == State::kAfterKey);
    if (!placed) {
      return fail(JsonStatus::kSyntax);
    }
    int16_t symbol = next_significant();
    const bool negative = symbol == '-';
    if (negative) {
      symbol = next_raw();
    }
    if ((symbol < '0') || (symbol > '9')) {
      return fail(missing_or(symbol, JsonStatus::kWrongType));
    }
    uint32_t magnitude = 0U;
    const JsonStatus read = read_digits(symbol, negative, magnitude);
    if (read != JsonStatus::kOk) {
      return fail(read);
    }
    out = signed_value(magnitude, negative);
    state_ = State::kAfterValue;
    return JsonStatus::kOk;
  }

  JsonStatus read_boolean(bool& out) {
    const bool placed = check(state_ != State::kFailed) && check(source_ != nullptr) &&
                        (state_ == State::kAfterKey);
    if (!placed) {
      return fail(JsonStatus::kSyntax);
    }
    const int16_t symbol = next_significant();
    const bool is_true = symbol == 't';
    if ((!is_true) && (symbol != 'f')) {
      return fail(missing_or(symbol, JsonStatus::kWrongType));
    }
    const bool matched =
        is_true ? match_rest(PSTR("rue"), kTrueRest) : match_rest(PSTR("alse"), kFalseRest);
    if (!matched) {
      return fail(JsonStatus::kSyntax);
    }
    out = is_true;
    state_ = State::kAfterValue;
    return JsonStatus::kOk;
  }

  JsonStatus finish() {
    const bool ended =
        check(state_ != State::kFailed) && check(source_ != nullptr) && (state_ == State::kEnded);
    if (!ended) {
      return fail(JsonStatus::kSyntax);
    }
    const int16_t symbol = next_significant();
    return (symbol < 0) ? JsonStatus::kOk : fail(JsonStatus::kSyntax);
  }

 private:
  enum class State : uint8_t { kStart, kFirstMember, kAfterKey, kAfterValue, kEnded, kFailed };

  static constexpr uint8_t kTrueRest = 3U;
  static constexpr uint8_t kFalseRest = 4U;
  static constexpr uint8_t kMaxDigits = 10U;
  static constexpr uint32_t kMaxPositive = 2147483647U;
  static constexpr uint32_t kMaxNegative = 2147483648U;
  static constexpr uint8_t kHexDigits = 4U;
  static constexpr uint16_t kSurrogateFirst = 0xD800U;
  static constexpr uint16_t kSurrogateLast = 0xDFFFU;
  static constexpr int16_t kSpace = 0x20;
  static constexpr int16_t kLastByte = 0xFF;

  static int32_t signed_value(const uint32_t magnitude, const bool negative) {
    const bool bounded =
        check(magnitude <= kMaxNegative) && check(negative || (magnitude <= kMaxPositive));
    if ((!bounded) || (magnitude == 0U)) {
      return 0;
    }
    if (!negative) {
      return static_cast<int32_t>(magnitude);
    }
    return -static_cast<int32_t>(magnitude - 1U) - 1;
  }

  static JsonStatus missing_or(const int16_t symbol, const JsonStatus otherwise) {
    const bool known = check(otherwise != JsonStatus::kOk) && check(otherwise != JsonStatus::kEnd);
    return ((symbol < 0) || (!known)) ? JsonStatus::kTruncated : otherwise;
  }

  JsonStatus fail(const JsonStatus status) {
    const bool is_error = check(status != JsonStatus::kOk) && check(status != JsonStatus::kEnd);
    if (state_ != State::kFailed) {
      error_ = is_error ? status : JsonStatus::kSyntax;
      state_ = State::kFailed;
    }
    return error_;
  }

  int16_t next_raw() {
    const bool wired = check(source_ != nullptr) && check(pushed_back_ <= kLastByte);
    if (!wired) {
      return -1;
    }
    if (pushed_back_ >= 0) {
      const int16_t symbol = pushed_back_;
      pushed_back_ = -1;
      return symbol;
    }
    return source_->next();
  }

  int16_t next_significant() {
    const bool wired = check(source_ != nullptr);
    int16_t symbol = -1;
    for (uint16_t i = 0U; wired && (i < kMaxStringScan); ++i) {
      symbol = next_raw();
      const bool blank =
          (symbol == ' ') || (symbol == '\t') || (symbol == '\n') || (symbol == '\r');
      if (!blank) {
        break;
      }
    }
    const bool in_range = check(symbol <= kLastByte);
    return in_range ? symbol : static_cast<int16_t>(-1);
  }

  int16_t member_start() {
    const bool placed = check(state_ != State::kFailed) &&
                        check((state_ == State::kFirstMember) || (state_ == State::kAfterValue));
    int16_t symbol = next_significant();
    if ((!placed) || (state_ != State::kAfterValue) || (symbol == '}')) {
      return symbol;
    }
    if (symbol != ',') {
      return (symbol < 0) ? symbol : static_cast<int16_t>('!');
    }
    symbol = next_significant();
    return (symbol == '}') ? static_cast<int16_t>('!') : symbol;
  }

  bool match_rest(const char* const rest, const uint8_t length) {
    const bool valid = check(rest != nullptr) && check(length <= kFalseRest);
    if (!valid) {
      return false;
    }
    bool matched = true;
    for (uint8_t i = 0U; i < length; ++i) {
      const int16_t symbol = next_raw();
      matched = matched && (symbol == static_cast<int16_t>(pgm_read_byte(rest + i)));
    }
    return matched && ends_value(next_raw());
  }

  bool ends_value(const int16_t symbol) {
    const bool delimiter = (symbol < 0) || (symbol == ',') || (symbol == '}') || (symbol == ' ') ||
                           (symbol == '\t') || (symbol == '\n') || (symbol == '\r');
    const bool slot_free = check(pushed_back_ < 0) && check(symbol <= kLastByte);
    if (delimiter && slot_free) {
      pushed_back_ = symbol;
    }
    return delimiter && slot_free;
  }

  JsonStatus read_digits(const int16_t first, const bool negative, uint32_t& magnitude) {
    const bool valid = check(first >= '0') && check(first <= '9');
    if (!valid) {
      return JsonStatus::kSyntax;
    }
    const uint32_t limit = negative ? kMaxNegative : kMaxPositive;
    magnitude = static_cast<uint32_t>(first - '0');
    int16_t symbol = next_raw();
    for (uint8_t i = 1U; (i <= kMaxDigits) && (symbol >= '0') && (symbol <= '9'); ++i) {
      const uint32_t digit = static_cast<uint32_t>(symbol - '0');
      const bool leading_zero = (i == 1U) && (first == '0');
      if (leading_zero || (magnitude > ((limit - digit) / 10U))) {
        return leading_zero ? JsonStatus::kSyntax : JsonStatus::kOutOfRange;
      }
      magnitude = (magnitude * 10U) + digit;
      symbol = next_raw();
    }
    const bool fraction = (symbol == '.') || (symbol == 'e') || (symbol == 'E');
    if (fraction) {
      return JsonStatus::kWrongType;
    }
    return ends_value(symbol) ? JsonStatus::kOk : JsonStatus::kSyntax;
  }

  JsonStatus read_string_body(char* const out, const uint8_t size) {
    const bool valid = check(out != nullptr) && check(size > 0U);
    if (!valid) {
      return JsonStatus::kSyntax;
    }
    length_ = 0U;
    continuation_ = 0U;
    for (uint16_t i = 0U; i < kMaxStringScan; ++i) {
      const int16_t symbol = next_raw();
      if (symbol == '"') {
        out[length_] = '\0';
        return (continuation_ == 0U) ? JsonStatus::kOk : JsonStatus::kSyntax;
      }
      const JsonStatus step = string_step(symbol, out, size);
      if (step != JsonStatus::kOk) {
        return step;
      }
    }
    return JsonStatus::kTooLong;
  }

  JsonStatus string_step(const int16_t symbol, char* const out, const uint8_t size) {
    const bool valid = check(out != nullptr) && check(length_ < size);
    if (!valid) {
      return JsonStatus::kTooLong;
    }
    if (symbol < 0) {
      return JsonStatus::kTruncated;
    }
    if (symbol < kSpace) {
      return JsonStatus::kSyntax;
    }
    if (symbol == '\\') {
      return (continuation_ == 0U) ? read_escape(out, size) : JsonStatus::kSyntax;
    }
    const bool well_formed = accept_utf8(static_cast<uint8_t>(symbol));
    return well_formed ? append(static_cast<uint8_t>(symbol), out, size) : JsonStatus::kSyntax;
  }

  bool accept_utf8(const uint8_t value) {
    const bool bounded = check(continuation_ <= 3U) && check(value >= kSpace);
    if (!bounded) {
      return false;
    }
    if (continuation_ > 0U) {
      const bool follows = (value >= 0x80U) && (value <= 0xBFU);
      continuation_ = static_cast<uint8_t>(continuation_ - 1U);
      return follows;
    }
    if (value < 0x80U) {
      return true;
    }
    continuation_ = continuation_for(value);
    return continuation_ > 0U;
  }

  static uint8_t continuation_for(const uint8_t lead) {
    const bool multibyte = check(lead >= 0x80U);
    if ((!multibyte) || (lead < 0xC2U)) {
      return 0U;
    }
    if (lead <= 0xDFU) {
      return 1U;
    }
    if (lead <= 0xEFU) {
      return 2U;
    }
    return (lead <= 0xF4U) ? 3U : 0U;
  }

  JsonStatus append(const uint8_t value, char* const out, const uint8_t size) {
    const bool valid = check(out != nullptr) && check(length_ < size);
    if ((!valid) || ((static_cast<uint16_t>(length_) + 1U) >= size)) {
      return JsonStatus::kTooLong;
    }
    out[length_] = static_cast<char>(value);
    length_ = static_cast<uint8_t>(length_ + 1U);
    return JsonStatus::kOk;
  }

  JsonStatus read_escape(char* const out, const uint8_t size) {
    const int16_t symbol = next_raw();
    const bool valid = check(out != nullptr) && check(size > 0U);
    if ((!valid) || (symbol < 0)) {
      return JsonStatus::kTruncated;
    }
    if (symbol == 'u') {
      return read_code_point(out, size);
    }
    const uint8_t plain = unescape(symbol);
    return (plain == 0U) ? JsonStatus::kSyntax : append(plain, out, size);
  }

  static uint8_t unescape(const int16_t symbol) {
    const bool printable = check(symbol >= 0) && check(symbol < 0x7F);
    if (!printable) {
      return 0U;
    }
    switch (symbol) {
      case '"': return '"';
      case '\\': return '\\';
      case '/': return '/';
      case 'b': return '\b';
      case 'f': return '\f';
      case 'n': return '\n';
      case 'r': return '\r';
      case 't': return '\t';
      default: return 0U;
    }
  }

  static int8_t hex_value(const int16_t symbol) {
    const bool in_range = check(symbol >= -1) && check(symbol <= kLastByte);
    if (!in_range) {
      return -1;
    }
    const bool digit = (symbol >= '0') && (symbol <= '9');
    const bool lower = (symbol >= 'a') && (symbol <= 'f');
    const bool upper = (symbol >= 'A') && (symbol <= 'F');
    if (digit) {
      return static_cast<int8_t>(symbol - '0');
    }
    if (lower || upper) {
      return static_cast<int8_t>((symbol - (lower ? 'a' : 'A')) + 10);
    }
    return -1;
  }

  JsonStatus read_code_point(char* const out, const uint8_t size) {
    const bool valid = check(out != nullptr) && check(size > 0U);
    uint16_t code = 0U;
    for (uint8_t i = 0U; valid && (i < kHexDigits); ++i) {
      const int8_t nibble = hex_value(next_raw());
      if (nibble < 0) {
        return JsonStatus::kSyntax;
      }
      const uint32_t shifted = static_cast<uint32_t>(code) << 4U;
      code = static_cast<uint16_t>(shifted | static_cast<uint32_t>(nibble));
    }
    const bool surrogate = (code >= kSurrogateFirst) && (code <= kSurrogateLast);
    if ((!valid) || (code == 0U) || surrogate) {
      return JsonStatus::kUnsupported;
    }
    return append_code_point(out, size, code);
  }

  JsonStatus append_code_point(char* const out, const uint8_t size, const uint16_t code) {
    const bool valid = check(out != nullptr) && check(size > 0U) && check(code != 0U);
    if (!valid) {
      return JsonStatus::kUnsupported;
    }
    const uint32_t wide = code;
    if (wide < 0x80U) {
      return append(static_cast<uint8_t>(wide), out, size);
    }
    const uint32_t tail = 0x80U | (wide & 0x3FU);
    if (wide < 0x800U) {
      const JsonStatus lead = append(static_cast<uint8_t>(0xC0U | (wide >> 6U)), out, size);
      return (lead == JsonStatus::kOk) ? append(static_cast<uint8_t>(tail), out, size) : lead;
    }
    const uint32_t middle = 0x80U | ((wide >> 6U) & 0x3FU);
    const JsonStatus lead = append(static_cast<uint8_t>(0xE0U | (wide >> 12U)), out, size);
    const JsonStatus next =
        (lead == JsonStatus::kOk) ? append(static_cast<uint8_t>(middle), out, size) : lead;
    return (next == JsonStatus::kOk) ? append(static_cast<uint8_t>(tail), out, size) : next;
  }

  SourceT* source_;
  State state_ = State::kStart;
  JsonStatus error_ = JsonStatus::kOk;
  int16_t pushed_back_ = -1;
  uint8_t length_ = 0U;
  uint8_t continuation_ = 0U;
};

}

#endif
