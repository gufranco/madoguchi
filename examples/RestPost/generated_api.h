// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef GENERATED_API_H
#define GENERATED_API_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <stdint.h>
#include <string.h>
#include <mado/Asset.h>
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
#include <mado/send_asset.h>

namespace rest_post {

enum RouteId : uint8_t {
  kCreateItem,
  kStaticAsset,
};

const char kRoutePath0[] PROGMEM = "/api/items";
const char kRoutePath1[] PROGMEM = "/*";

const mado::Route kRoutes[] PROGMEM = {
    {&kRoutePath0[0], mado::Method::kPost, kCreateItem},
    {&kRoutePath1[0], mado::Method::kGet, kStaticAsset},
    {nullptr, mado::Method::kAny, 0U},
};

const char kHeaderName0[] PROGMEM = "Content-Type";
const char kHeaderName1[] PROGMEM = "Accept-Encoding";
const char kHeaderName2[] PROGMEM = "If-None-Match";
const char* const kHeaderNames[] PROGMEM = {&kHeaderName0[0], &kHeaderName1[0], &kHeaderName2[0], nullptr};

struct Limits {
  static constexpr uint16_t kLineBufferSize = 128U;
  static constexpr uint16_t kOutputBufferSize = 32U;
  static constexpr uint8_t kHeaderPoolSize = 96U;
  static constexpr uint8_t kHeaderNameSize = 32U;
  static constexpr uint8_t kMaxCapturedHeaders = 8U;
  static constexpr uint8_t kMaxHeaderLines = 48U;
  static constexpr uint8_t kMaxRoutes = 2U;
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 100000UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
  static constexpr uint16_t kMaxDrainBytes = 1024U;
  static constexpr uint16_t kFileChunkSize = 32U;
};

inline bool route_needs_token(const uint8_t route) {
  const bool known = mado::check(route <= kStaticAsset);
  if (!known) {
    return true;
  }
  switch (route) {
    default:
      return false;
  }
}

inline bool route_path_id(const char* const path, const uint8_t route, uint8_t& id) {
  id = 0U;
  const bool valid = mado::check(path != nullptr) && mado::check(route <= kStaticAsset);
  if (!valid) {
    return false;
  }
  switch (route) {
    default:
      return false;
  }
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

template <typename SourceT>
mado::JsonStatus read_item_name(mado::JsonReader<SourceT>& reader, Item& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemName) == 0U);
  present = static_cast<uint8_t>(present | kItemName);
  if (!fresh) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_string(&value.name[0], sizeof(value.name));
}

template <typename SourceT>
mado::JsonStatus read_item_on(mado::JsonReader<SourceT>& reader, Item& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kItemOn) == 0U);
  present = static_cast<uint8_t>(present | kItemOn);
  if (!fresh) {
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
  if ((present & kItemName) == 0U) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: name")};
  }
  if ((present & kItemOn) == 0U) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: on")};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_item_body(ExchangeT& exchange, Item& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (framing.status != mado::http::kNoResponse) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_item(reader, value, present);
  if (status != mado::JsonStatus::kOk) {
    return mado::json_problem(status);
  }
  return item_missing(present);
}

template <typename ExchangeT>
bool write_item(ExchangeT& exchange, const Item& value) {
  const bool sound = mado::check((value.name[sizeof(value.name) - 1U] == '\0') && (value.id >= 1) && (value.id <= 254));
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

const uint8_t kAsset0[611] PROGMEM = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0xb5, 0x54,
    0xd1, 0x6e, 0xd3, 0x30, 0x14, 0xfd, 0x15, 0xcb, 0xf0, 0x46, 0x58, 0xd2,
    0xae, 0x1d, 0x5a, 0x79, 0x1a, 0x88, 0x87, 0x21, 0x4a, 0x27, 0xd6, 0x37,
    0xc4, 0x83, 0x1b, 0xdf, 0x34, 0xde, 0x1c, 0xdb, 0xd8, 0x37, 0x6c, 0x55,
    0x95, 0x7f, 0xe7, 0xda, 0x29, 0x6d, 0xa5, 0x8c, 0x21, 0x04, 0xed, 0x43,
    0xe3, 0x38, 0xf6, 0x39, 0xe7, 0xde, 0x73, 0xec, 0x2d, 0xb7, 0x0e, 0x8c,
    0x70, 0x8a, 0xcf, 0xf8, 0xf9, 0xd9, 0xe8, 0xac, 0xe0, 0x19, 0x57, 0xa6,
    0xb2, 0x7c, 0xb6, 0xe5, 0xa8, 0x50, 0x03, 0xcd, 0xcf, 0x85, 0xb4, 0xeb,
    0xb6, 0xac, 0x15, 0xbb, 0x59, 0xdc, 0x2e, 0x19, 0x3c, 0x8a, 0xc6, 0xd1,
    0x87, 0x8c, 0xff, 0x00, 0x1f, 0x94, 0x35, 0xb4, 0x84, 0x36, 0xa6, 0xad,
    0x12, 0x42, 0xe9, 0x95, 0xc3, 0x7e, 0xf6, 0xbd, 0x07, 0x81, 0x10, 0x98,
    0x42, 0x68, 0xe8, 0xdf, 0xb0, 0x2f, 0x57, 0xf3, 0xb7, 0x0c, 0x6b, 0x60,
    0x28, 0x56, 0x1a, 0x58, 0x6d, 0xb5, 0x0c, 0xac, 0xb2, 0xad, 0x3f, 0xe3,
    0x5d, 0xc6, 0x03, 0xf8, 0x88, 0xc8, 0x67, 0x5f, 0xb7, 0xbc, 0xf5, 0x9a,
    0x00, 0x72, 0xde, 0x7d, 0xcb, 0x78, 0x69, 0x1b, 0x67, 0x0d, 0x18, 0x0c,
    0x51, 0x55, 0x28, 0x6b, 0x68, 0x44, 0x1a, 0x5e, 0x13, 0x6e, 0x12, 0xba,
    0x71, 0x51, 0xa7, 0x5d, 0xdd, 0x41, 0x89, 0xa4, 0x42, 0x48, 0xa9, 0xa2,
    0x04, 0xa1, 0x6f, 0x3c, 0x95, 0xe7, 0x51, 0x01, 0xad, 0xaf, 0x84, 0x0e,
    0x90, 0x71, 0x0f, 0xdf, 0x5b, 0xe5, 0x41, 0x12, 0x0d, 0x37, 0xa2, 0x89,
    0x75, 0x90, 0x58, 0xa2, 0x71, 0x47, 0x6b, 0xb7, 0x5c, 0xc9, 0x23, 0x64,
    0x65, 0x10, 0xd6, 0xe0, 0x69, 0x69, 0xa3, 0x8c, 0x6a, 0x5a, 0x62, 0x1d,
    0xd1, 0x58, 0x3c, 0xf6, 0xe3, 0xf1, 0x74, 0x12, 0x71, 0x85, 0x5c, 0x18,
    0xbd, 0xe1, 0x33, 0xf4, 0x2d, 0x50, 0x39, 0x09, 0xfc, 0x80, 0x11, 0xd0,
    0x2b, 0xb3, 0xe6, 0x69, 0xdb, 0x27, 0x30, 0x6b, 0xac, 0x09, 0xe4, 0xa2,
    0x4b, 0xec, 0x87, 0x55, 0x2b, 0x6b, 0x35, 0x08, 0xc3, 0xbb, 0x8e, 0xbe,
    0x90, 0x7a, 0x6a, 0xd3, 0xbf, 0x95, 0xd8, 0x9b, 0x48, 0xcd, 0x45, 0x81,
    0x6d, 0x18, 0xd6, 0xb9, 0x33, 0xf9, 0x59, 0x99, 0x93, 0xa2, 0xdb, 0x03,
    0x3c, 0xdb, 0x94, 0x49, 0x51, 0x1c, 0xb5, 0x65, 0x7a, 0x79, 0xd9, 0xc5,
    0x48, 0xa0, 0x50, 0xfa, 0x0f, 0x0c, 0x17, 0x93, 0xae, 0x4b, 0x25, 0x3b,
    0x81, 0x75, 0x22, 0xc9, 0x29, 0x93, 0x79, 0x0a, 0x4e, 0x7c, 0x73, 0x36,
    0x60, 0x7c, 0x46, 0xe5, 0x22, 0xd6, 0x7d, 0x4d, 0xc5, 0xf1, 0x32, 0xe5,
    0x2b, 0xa5, 0x80, 0xf4, 0xb5, 0x4d, 0x23, 0xfc, 0x66, 0x1f, 0x3b, 0x26,
    0x4c, 0x0a, 0x1e, 0xef, 0xfb, 0x01, 0x01, 0xdf, 0x59, 0xb9, 0x89, 0x20,
    0x87, 0xf6, 0x44, 0xa7, 0x62, 0xbe, 0xa8, 0x12, 0x93, 0xf0, 0x85, 0x73,
    0x5a, 0x95, 0x89, 0x21, 0xbf, 0x0b, 0xbd, 0x33, 0x7d, 0xe0, 0xe2, 0xe8,
    0xa5, 0x87, 0x8a, 0xf0, 0x5f, 0xe4, 0x87, 0x44, 0xe6, 0xbb, 0x38, 0xe6,
    0x49, 0x45, 0x5f, 0x83, 0x87, 0x40, 0x5f, 0x43, 0xdf, 0xdf, 0x71, 0x31,
    0x8a, 0x8f, 0xa7, 0x0e, 0x86, 0xe4, 0xff, 0x9f, 0x9b, 0x1c, 0x18, 0xd2,
    0xcd, 0x85, 0xae, 0xac, 0x6f, 0x40, 0xb2, 0x8f, 0xb7, 0x8b, 0xcf, 0xcc,
    0x7a, 0xd6, 0xa8, 0x10, 0xc8, 0x04, 0xb6, 0x8a, 0x2d, 0xf9, 0xad, 0x0a,
    0xd7, 0xe7, 0xef, 0xd5, 0xdf, 0xa9, 0xf9, 0x95, 0xda, 0x9d, 0xa0, 0xd1,
    0xf9, 0x50, 0xd0, 0x92, 0xae, 0x80, 0x48, 0xcd, 0x54, 0x60, 0x5a, 0x78,
    0x4a, 0x11, 0xdd, 0x0a, 0xe4, 0x17, 0xd6, 0x34, 0xb1, 0xf7, 0x98, 0x89,
    0xb2, 0x04, 0x47, 0xa7, 0xfe, 0xc4, 0x02, 0xa7, 0x4f, 0x18, 0xd4, 0xf3,
    0xbd, 0x5e, 0x52, 0x64, 0xa3, 0x48, 0x63, 0x91, 0x0d, 0xfc, 0x39, 0xad,
    0xac, 0xf1, 0x78, 0x28, 0xeb, 0x8a, 0x55, 0x0a, 0xb4, 0x8c, 0x8a, 0x76,
    0x0e, 0x66, 0xac, 0x35, 0xf7, 0xc6, 0x3e, 0x98, 0x8c, 0xa1, 0xb5, 0x4c,
    0x5b, 0x32, 0x95, 0xfc, 0xb5, 0x55, 0xba, 0x66, 0x1f, 0x7c, 0x7c, 0x4f,
    0xe7, 0xee, 0xa4, 0x62, 0xa7, 0xc5, 0x9b, 0xa1, 0xd8, 0x0f, 0x74, 0x99,
    0x6f, 0x58, 0xd0, 0xd4, 0x3a, 0xd2, 0x8b, 0xe2, 0x1e, 0x4e, 0xda, 0xb2,
    0xf4, 0xfb, 0x09, 0x6e, 0x45, 0x30, 0xd1, 0xcf, 0x06, 0x00, 0x00};

constexpr uint8_t kNoAsset = 255U;

inline uint8_t find_asset(const char* const path) {
  const bool valid = mado::check(path != nullptr) && mado::check(path[0] == '/');
  if (!valid) {
    return kNoAsset;
  }
  if (strcmp_P(path, PSTR("/api/openapi.json")) == 0) {
    return 0U;
  }
  return kNoAsset;
}

inline mado::Asset asset_at(const uint8_t index) {
  const bool known = mado::check(index < 1U);
  const mado::Asset missing = {nullptr, 0U, nullptr, nullptr, nullptr};
  if (!known) {
    return missing;
  }
  switch (index) {
    case 0U:
      return mado::Asset{&kAsset0[0], sizeof(kAsset0), F("Content-Type: application/json\r\nContent-Encoding: gzip\r\nContent-Length: 611\r\nETag: \"6f7b08979c559061\"\r\nCache-Control: max-age=300, must-revalidate\r\nVary: Accept-Encoding\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"), F("ETag: \"6f7b08979c559061\"\r\nCache-Control: max-age=300, must-revalidate\r\nVary: Accept-Encoding\r\n"), F("\"6f7b08979c559061\"")};
    default:
      return missing;
  }
}

template <typename ExchangeT>
bool send_static_asset(ExchangeT& exchange) {
  const uint8_t index = find_asset(exchange.path());
  const bool found = mado::check(index == kNoAsset) || mado::check(index < kNoAsset);
  if ((!found) || (index == kNoAsset)) {
    return exchange.send_empty(mado::http::kNotFound);
  }
  return mado::send_asset(exchange, asset_at(index));
}

}

#endif
