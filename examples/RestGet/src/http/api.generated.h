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

namespace rest_get {

constexpr uint8_t kListItems = 0U;
constexpr uint8_t kGetItem = 1U;
constexpr uint8_t kOpenApiDocument = 2U;
constexpr uint8_t kCardFile = 3U;

const char kRoutePath0[] PROGMEM = "/api/openapi.json";
const char kRoutePath1[] PROGMEM = "/api/items";
const char kRoutePath2[] PROGMEM = "/api/items/*";
const char kRoutePath3[] PROGMEM = "/*";

const mado::Route kRoutes[] PROGMEM = {
    {&kRoutePath0[0], mado::Method::kGet, kOpenApiDocument},
    {&kRoutePath1[0], mado::Method::kGet, kListItems},
    {&kRoutePath2[0], mado::Method::kGet, kGetItem},
    {&kRoutePath3[0], mado::Method::kGet, kCardFile},
    {nullptr, mado::Method::kAny, 0U},
};

const char kHeaderName0[] PROGMEM = "Accept-Encoding";
const char kHeaderName1[] PROGMEM = "If-None-Match";
const char* const kHeaderNames[] PROGMEM = {&kHeaderName0[0], &kHeaderName1[0], nullptr};

struct Limits {
  static constexpr uint16_t kLineBufferSize = 128U;
  static constexpr uint16_t kOutputBufferSize = 32U;
  static constexpr uint8_t kHeaderPoolSize = 96U;
  static constexpr uint8_t kHeaderNameSize = 32U;
  static constexpr uint8_t kMaxCapturedHeaders = 8U;
  static constexpr uint8_t kMaxHeaderLines = 48U;
  static constexpr uint8_t kMaxRoutes = 4U;
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 100000UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
  static constexpr uint16_t kMaxDrainBytes = 1024U;
  static constexpr uint16_t kFileChunkSize = 32U;
};

inline bool route_path_id(const char* const path, const uint8_t route,
                          uint8_t& resource_id) {
  resource_id = 0U;
  const bool valid = mado::check(path != nullptr) && mado::check(route <= kCardFile);
  if (!valid) {
    return false;
  }
  if ((route == kGetItem)) {
    return mado::parse_path_id(path, PSTR("/api/items/"), resource_id);
  }
  return false;
}

struct Item {
  int32_t id;
  char name[17];
  bool on;
};

constexpr uint8_t kItemId = 0x01U;
constexpr uint8_t kItemName = 0x02U;
constexpr uint8_t kItemOn = 0x04U;
constexpr uint8_t kItemRequired = 0x06U;
constexpr uint8_t kItemWritable = 0x06U;
constexpr uint8_t kItemKeySize = 5U;
constexpr uint8_t kItemFieldCount = 3U;

template <typename ExchangeT>
bool write_item(ExchangeT& exchange, const Item& value) {
  const bool sound = mado::check((value.name[sizeof(value.name) - 1U] == '\0')) &&
                     mado::check((value.id >= 1) && (value.id <= 254));
  bool written = sound;
  written = written && (exchange.print(F("{\"id\":")) > 0U);
  written = written && (exchange.print_signed(value.id) > 0U);
  written = written && (exchange.print(F(",\"name\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.name[0]);
  written = written && (exchange.print(F(",\"on\":")) > 0U);
  written = written && (exchange.print(value.on ? F("true") : F("false")) > 0U);
  return written && (exchange.print(F("}")) > 0U);
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
