// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef GENERATED_API_H
#define GENERATED_API_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <stdint.h>
#include <string.h>
#include <mado/BodySource.h>
#include <mado/JsonReader.h>
#include <mado/JsonStatus.h>
#include <mado/Problem.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/json_body_problem.h>
#include <mado/json_problem.h>
#include <mado/json_write_string.h>
#include <mado/parse_path_id.h>
#include <mado/send_problem.h>
#include <mado/serve_card_file.h>

namespace rest_api {

constexpr uint8_t kLogin = 0U;
constexpr uint8_t kListItems = 1U;
constexpr uint8_t kCreateItem = 2U;
constexpr uint8_t kGetItem = 3U;
constexpr uint8_t kReplaceItem = 4U;
constexpr uint8_t kUpdateItem = 5U;
constexpr uint8_t kDeleteItem = 6U;
constexpr uint8_t kReadLog = 7U;
constexpr uint8_t kClearLog = 8U;
constexpr uint8_t kOpenApiDocument = 9U;
constexpr uint8_t kCardFile = 10U;

const char kRoutePath0[] PROGMEM = "/api/openapi.json";
const char kRoutePath1[] PROGMEM = "/api/auth/login";
const char kRoutePath2[] PROGMEM = "/api/items";
const char kRoutePath3[] PROGMEM = "/api/log";
const char kRoutePath4[] PROGMEM = "/api/items/*";
const char kRoutePath5[] PROGMEM = "/*";

const mado::Route kRoutes[] PROGMEM = {
    {&kRoutePath0[0], mado::Method::kGet, kOpenApiDocument},
    {&kRoutePath1[0], mado::Method::kPost, kLogin},
    {&kRoutePath2[0], mado::Method::kGet, kListItems},
    {&kRoutePath2[0], mado::Method::kPost, kCreateItem},
    {&kRoutePath3[0], mado::Method::kGet, kReadLog},
    {&kRoutePath3[0], mado::Method::kDelete, kClearLog},
    {&kRoutePath4[0], mado::Method::kGet, kGetItem},
    {&kRoutePath4[0], mado::Method::kPut, kReplaceItem},
    {&kRoutePath4[0], mado::Method::kPatch, kUpdateItem},
    {&kRoutePath4[0], mado::Method::kDelete, kDeleteItem},
    {&kRoutePath5[0], mado::Method::kGet, kCardFile},
    {nullptr, mado::Method::kAny, 0U},
};

const char kHeaderName0[] PROGMEM = "Authorization";
const char kHeaderName1[] PROGMEM = "Content-Type";
const char kHeaderName2[] PROGMEM = "Accept-Encoding";
const char kHeaderName3[] PROGMEM = "If-None-Match";
const char kHeaderName4[] PROGMEM = "Idempotency-Key";
const char kHeaderName5[] PROGMEM = "If-Match";
const char kHeaderName6[] PROGMEM = "Range";
const char* const kHeaderNames[] PROGMEM = {&kHeaderName0[0], &kHeaderName1[0], &kHeaderName2[0], &kHeaderName3[0], &kHeaderName4[0], &kHeaderName5[0], &kHeaderName6[0], nullptr};

struct Limits {
  static constexpr uint16_t kLineBufferSize = 128U;
  static constexpr uint16_t kOutputBufferSize = 32U;
  static constexpr uint8_t kHeaderPoolSize = 240U;
  static constexpr uint8_t kHeaderNameSize = 32U;
  static constexpr uint8_t kMaxCapturedHeaders = 8U;
  static constexpr uint8_t kMaxHeaderLines = 48U;
  static constexpr uint8_t kMaxRoutes = 11U;
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 100000UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
  static constexpr uint16_t kMaxDrainBytes = 1024U;
  static constexpr uint16_t kFileChunkSize = 32U;
};

inline bool route_needs_token(const uint8_t route) {
  const bool known = mado::check(route <= kCardFile);
  return (!known) || (route == kCreateItem) || (route == kReplaceItem) || (route == kUpdateItem) || (route == kDeleteItem) || (route == kReadLog) || (route == kClearLog);
}

inline bool route_path_id(const char* const path, const uint8_t route,
                          uint8_t& resource_id) {
  resource_id = 0U;
  const bool valid = mado::check(path != nullptr) && mado::check(route <= kCardFile);
  if (!valid) {
    return false;
  }
  if ((route == kGetItem) || (route == kReplaceItem) || (route == kUpdateItem) || (route == kDeleteItem)) {
    return mado::parse_path_id(path, PSTR("/api/items/"), resource_id);
  }
  return false;
}

inline const __FlashStringHelper* get_item_url() {
  return F("/api/items/");
}

struct Problem {
  char title[41];
  int32_t status;
  char detail[65];
};

constexpr uint8_t kProblemTitle = 0x01U;
constexpr uint8_t kProblemStatus = 0x02U;
constexpr uint8_t kProblemDetail = 0x04U;
constexpr uint8_t kProblemRequired = 0x03U;
constexpr uint8_t kProblemWritable = 0x07U;
constexpr uint8_t kProblemKeySize = 7U;
constexpr uint8_t kProblemFieldCount = 3U;

struct Credentials {
  char username[17];
  char password[65];
};

constexpr uint8_t kCredentialsUsername = 0x01U;
constexpr uint8_t kCredentialsPassword = 0x02U;
constexpr uint8_t kCredentialsRequired = 0x03U;
constexpr uint8_t kCredentialsWritable = 0x03U;
constexpr uint8_t kCredentialsKeySize = 9U;
constexpr uint8_t kCredentialsFieldCount = 2U;

template <typename SourceT>
mado::JsonStatus read_credentials_username(mado::JsonReader<SourceT>& reader, Credentials& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kCredentialsUsername) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kCredentialsWritable)) == 0U);
  present = static_cast<uint8_t>(present | kCredentialsUsername);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_string(&value.username[0], sizeof(value.username));
}

template <typename SourceT>
mado::JsonStatus read_credentials_password(mado::JsonReader<SourceT>& reader, Credentials& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kCredentialsPassword) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kCredentialsWritable)) == 0U);
  present = static_cast<uint8_t>(present | kCredentialsPassword);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_string(&value.password[0], sizeof(value.password));
}

template <typename SourceT>
mado::JsonStatus read_credentials_member(mado::JsonReader<SourceT>& reader, const char* const key,
                                    Credentials& value, uint8_t& present) {
  const bool valid = mado::check(key != nullptr) && mado::check((present & static_cast<uint8_t>(~kCredentialsWritable)) == 0U);
  if (!valid) {
    return mado::JsonStatus::kSyntax;
  }
  if (strcmp_P(key, PSTR("username")) == 0) {
    return read_credentials_username(reader, value, present);
  }
  if (strcmp_P(key, PSTR("password")) == 0) {
    return read_credentials_password(reader, value, present);
  }
  return mado::JsonStatus::kUnsupported;
}

template <typename SourceT>
mado::JsonStatus read_credentials(mado::JsonReader<SourceT>& reader, Credentials& value,
                                uint8_t& present) {
  present = 0U;
  mado::JsonStatus status = reader.begin_object();
  for (uint8_t i = 0U; (status == mado::JsonStatus::kOk) && (i <= kCredentialsFieldCount); ++i) {
    char key[kCredentialsKeySize] = {};
    status = reader.next_key(&key[0], sizeof(key));
    if (status == mado::JsonStatus::kEnd) {
      return reader.finish();
    }
    if (status == mado::JsonStatus::kOk) {
      status = read_credentials_member(reader, &key[0], value, present);
    }
  }
  const bool failed = mado::check(status != mado::JsonStatus::kEnd);
  return (failed && (status != mado::JsonStatus::kOk)) ? status : mado::JsonStatus::kUnsupported;
}

inline mado::Problem credentials_missing(const uint8_t present) {
  const bool sound = mado::check((present & static_cast<uint8_t>(~kCredentialsWritable)) == 0U);
  if (!sound) {
    return mado::Problem{mado::http::kUnprocessableContent, nullptr};
  }
  if (!mado::check((present & kCredentialsUsername) != 0U)) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: username")};
  }
  if (!mado::check((present & kCredentialsPassword) != 0U)) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: password")};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_credentials_body(ExchangeT& exchange, Credentials& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (!mado::check(framing.status == mado::http::kNoResponse)) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_credentials(reader, value, present);
  if (!mado::check(status == mado::JsonStatus::kOk)) {
    return mado::json_problem(status);
  }
  return credentials_missing(present);
}

struct Token {
  char access_token[201];
  char token_type[7];
  int32_t expires_in;
};

constexpr uint8_t kTokenAccessToken = 0x01U;
constexpr uint8_t kTokenTokenType = 0x02U;
constexpr uint8_t kTokenExpiresIn = 0x04U;
constexpr uint8_t kTokenRequired = 0x07U;
constexpr uint8_t kTokenWritable = 0x07U;
constexpr uint8_t kTokenKeySize = 13U;
constexpr uint8_t kTokenFieldCount = 3U;

template <typename ExchangeT>
bool write_token(ExchangeT& exchange, const Token& value) {
  const bool sound = mado::check((value.access_token[sizeof(value.access_token) - 1U] == '\0')) &&
                     mado::check((value.token_type[sizeof(value.token_type) - 1U] == '\0')) &&
                     mado::check((value.expires_in >= 1) && (value.expires_in <= 900));
  bool written = sound;
  written = written && (exchange.print(F("{\"access_token\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.access_token[0]);
  written = written && (exchange.print(F(",\"token_type\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.token_type[0]);
  written = written && (exchange.print(F(",\"expires_in\":")) > 0U);
  written = written && (exchange.print_signed(value.expires_in) > 0U);
  return written && (exchange.print(F("}")) > 0U);
}

struct Item {
  int32_t id;
  char name[17];
  int32_t value;
  bool on;
};

constexpr uint8_t kItemId = 0x01U;
constexpr uint8_t kItemName = 0x02U;
constexpr uint8_t kItemValue = 0x04U;
constexpr uint8_t kItemOn = 0x08U;
constexpr uint8_t kItemRequired = 0x0EU;
constexpr uint8_t kItemWritable = 0x0EU;
constexpr uint8_t kItemKeySize = 6U;
constexpr uint8_t kItemFieldCount = 4U;

template <typename SourceT>
mado::JsonStatus read_item_name(mado::JsonReader<SourceT>& reader, Item& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemName) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemName);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  const mado::JsonStatus status =
      reader.read_string(&value.name[0], sizeof(value.name));
  const bool long_enough = mado::check(
      strnlen(&value.name[0], sizeof(value.name)) >= 1U);
  const bool too_short = (status == mado::JsonStatus::kOk) && (!long_enough);
  return too_short ? mado::JsonStatus::kTooShort : status;
}

template <typename SourceT>
mado::JsonStatus read_item_value(mado::JsonReader<SourceT>& reader, Item& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemValue) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemValue);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  int32_t number = 0;
  const mado::JsonStatus status = reader.read_integer(number);
  const bool in_range = mado::check((number >= -100000) && (number <= 100000));
  value.value = in_range ? number : value.value;
  const bool rejected = (status == mado::JsonStatus::kOk) && (!in_range);
  return rejected ? mado::JsonStatus::kOutOfRange : status;
}

template <typename SourceT>
mado::JsonStatus read_item_on(mado::JsonReader<SourceT>& reader, Item& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemOn) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemOn);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_boolean(value.on);
}

template <typename SourceT>
mado::JsonStatus read_item_member(mado::JsonReader<SourceT>& reader, const char* const key,
                                    Item& value, uint8_t& present) {
  const bool valid = mado::check(key != nullptr) && mado::check((present & static_cast<uint8_t>(~kItemWritable)) == 0U);
  if (!valid) {
    return mado::JsonStatus::kSyntax;
  }
  if (strcmp_P(key, PSTR("name")) == 0) {
    return read_item_name(reader, value, present);
  }
  if (strcmp_P(key, PSTR("value")) == 0) {
    return read_item_value(reader, value, present);
  }
  if (strcmp_P(key, PSTR("on")) == 0) {
    return read_item_on(reader, value, present);
  }
  return mado::JsonStatus::kUnsupported;
}

template <typename SourceT>
mado::JsonStatus read_item(mado::JsonReader<SourceT>& reader, Item& value,
                                uint8_t& present) {
  present = 0U;
  mado::JsonStatus status = reader.begin_object();
  for (uint8_t i = 0U; (status == mado::JsonStatus::kOk) && (i <= kItemFieldCount); ++i) {
    char key[kItemKeySize] = {};
    status = reader.next_key(&key[0], sizeof(key));
    if (status == mado::JsonStatus::kEnd) {
      return reader.finish();
    }
    if (status == mado::JsonStatus::kOk) {
      status = read_item_member(reader, &key[0], value, present);
    }
  }
  const bool failed = mado::check(status != mado::JsonStatus::kEnd);
  return (failed && (status != mado::JsonStatus::kOk)) ? status : mado::JsonStatus::kUnsupported;
}

inline mado::Problem item_missing(const uint8_t present) {
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemWritable)) == 0U);
  if (!sound) {
    return mado::Problem{mado::http::kUnprocessableContent, nullptr};
  }
  if (!mado::check((present & kItemName) != 0U)) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: name")};
  }
  if (!mado::check((present & kItemValue) != 0U)) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: value")};
  }
  if (!mado::check((present & kItemOn) != 0U)) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: on")};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_item_body(ExchangeT& exchange, Item& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (!mado::check(framing.status == mado::http::kNoResponse)) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_item(reader, value, present);
  if (!mado::check(status == mado::JsonStatus::kOk)) {
    return mado::json_problem(status);
  }
  return item_missing(present);
}

template <typename ExchangeT>
bool write_item(ExchangeT& exchange, const Item& value) {
  const bool sound = mado::check((value.name[sizeof(value.name) - 1U] == '\0')) &&
                     mado::check((value.id >= 1) && (value.id <= 254)) &&
                     mado::check((value.value >= -100000) && (value.value <= 100000));
  bool written = sound;
  written = written && (exchange.print(F("{\"id\":")) > 0U);
  written = written && (exchange.print_signed(value.id) > 0U);
  written = written && (exchange.print(F(",\"name\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.name[0]);
  written = written && (exchange.print(F(",\"value\":")) > 0U);
  written = written && (exchange.print_signed(value.value) > 0U);
  written = written && (exchange.print(F(",\"on\":")) > 0U);
  written = written && (exchange.print(value.on ? F("true") : F("false")) > 0U);
  return written && (exchange.print(F("}")) > 0U);
}

struct ItemPatch {
  char name[17];
  int32_t value;
  bool on;
};

constexpr uint8_t kItemPatchName = 0x01U;
constexpr uint8_t kItemPatchValue = 0x02U;
constexpr uint8_t kItemPatchOn = 0x04U;
constexpr uint8_t kItemPatchRequired = 0x00U;
constexpr uint8_t kItemPatchWritable = 0x07U;
constexpr uint8_t kItemPatchKeySize = 6U;
constexpr uint8_t kItemPatchFieldCount = 3U;

template <typename SourceT>
mado::JsonStatus read_item_patch_name(mado::JsonReader<SourceT>& reader, ItemPatch& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemPatchName) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemPatchWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemPatchName);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  const mado::JsonStatus status =
      reader.read_string(&value.name[0], sizeof(value.name));
  const bool long_enough = mado::check(
      strnlen(&value.name[0], sizeof(value.name)) >= 1U);
  const bool too_short = (status == mado::JsonStatus::kOk) && (!long_enough);
  return too_short ? mado::JsonStatus::kTooShort : status;
}

template <typename SourceT>
mado::JsonStatus read_item_patch_value(mado::JsonReader<SourceT>& reader, ItemPatch& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemPatchValue) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemPatchWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemPatchValue);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  int32_t number = 0;
  const mado::JsonStatus status = reader.read_integer(number);
  const bool in_range = mado::check((number >= -100000) && (number <= 100000));
  value.value = in_range ? number : value.value;
  const bool rejected = (status == mado::JsonStatus::kOk) && (!in_range);
  return rejected ? mado::JsonStatus::kOutOfRange : status;
}

template <typename SourceT>
mado::JsonStatus read_item_patch_on(mado::JsonReader<SourceT>& reader, ItemPatch& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemPatchOn) == 0U);
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemPatchWritable)) == 0U);
  present = static_cast<uint8_t>(present | kItemPatchOn);
  if ((!fresh) || (!sound)) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_boolean(value.on);
}

template <typename SourceT>
mado::JsonStatus read_item_patch_member(mado::JsonReader<SourceT>& reader, const char* const key,
                                    ItemPatch& value, uint8_t& present) {
  const bool valid = mado::check(key != nullptr) && mado::check((present & static_cast<uint8_t>(~kItemPatchWritable)) == 0U);
  if (!valid) {
    return mado::JsonStatus::kSyntax;
  }
  if (strcmp_P(key, PSTR("name")) == 0) {
    return read_item_patch_name(reader, value, present);
  }
  if (strcmp_P(key, PSTR("value")) == 0) {
    return read_item_patch_value(reader, value, present);
  }
  if (strcmp_P(key, PSTR("on")) == 0) {
    return read_item_patch_on(reader, value, present);
  }
  return mado::JsonStatus::kUnsupported;
}

template <typename SourceT>
mado::JsonStatus read_item_patch(mado::JsonReader<SourceT>& reader, ItemPatch& value,
                                uint8_t& present) {
  present = 0U;
  mado::JsonStatus status = reader.begin_object();
  for (uint8_t i = 0U; (status == mado::JsonStatus::kOk) && (i <= kItemPatchFieldCount); ++i) {
    char key[kItemPatchKeySize] = {};
    status = reader.next_key(&key[0], sizeof(key));
    if (status == mado::JsonStatus::kEnd) {
      return reader.finish();
    }
    if (status == mado::JsonStatus::kOk) {
      status = read_item_patch_member(reader, &key[0], value, present);
    }
  }
  const bool failed = mado::check(status != mado::JsonStatus::kEnd);
  return (failed && (status != mado::JsonStatus::kOk)) ? status : mado::JsonStatus::kUnsupported;
}

inline mado::Problem item_patch_missing(const uint8_t present) {
  const bool sound = mado::check((present & static_cast<uint8_t>(~kItemPatchWritable)) == 0U);
  if (!sound) {
    return mado::Problem{mado::http::kUnprocessableContent, nullptr};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_item_patch_body(ExchangeT& exchange, ItemPatch& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (!mado::check(framing.status == mado::http::kNoResponse)) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_item_patch(reader, value, present);
  if (!mado::check(status == mado::JsonStatus::kOk)) {
    return mado::json_problem(status);
  }
  return item_patch_missing(present);
}

template <typename ExchangeT>
bool write_item_patch(ExchangeT& exchange, const ItemPatch& value) {
  const bool sound = mado::check((value.name[sizeof(value.name) - 1U] == '\0')) &&
                     mado::check((value.value >= -100000) && (value.value <= 100000));
  bool written = sound;
  written = written && (exchange.print(F("{\"name\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.name[0]);
  written = written && (exchange.print(F(",\"value\":")) > 0U);
  written = written && (exchange.print_signed(value.value) > 0U);
  written = written && (exchange.print(F(",\"on\":")) > 0U);
  written = written && (exchange.print(value.on ? F("true") : F("false")) > 0U);
  return written && (exchange.print(F("}")) > 0U);
}

const char kOpenApiCardPath[] PROGMEM = "/openapi.json";

template <uint16_t kReadMode, typename ExchangeT, typename SdT>
bool send_card(ExchangeT& exchange, SdT* const card, const uint8_t route) {
  const bool served = mado::check(route == kOpenApiDocument) ||
                      mado::check(route == kCardFile);
  if (!served) {
    return exchange.send_empty(mado::http::kNotFound);
  }
  if (card == nullptr) {
    const mado::Problem missing{
        mado::http::kServiceUnavailable,
        F("the SD card is not available"),
    };
    return mado::send_problem(exchange, missing);
  }
  if (route == kOpenApiDocument) {
    return mado::serve_card_path<kReadMode>(exchange, *card, &kOpenApiCardPath[0]);
  }
  return mado::serve_card_file<kReadMode>(exchange, *card);
}

}

#endif
