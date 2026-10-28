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

namespace rest_format {

enum RouteId : uint8_t {
  kGetCard,
  kGetFormat,
  kStartFormat,
};

const char kRoutePath0[] PROGMEM = "/api/card";
const char kRoutePath1[] PROGMEM = "/api/card/format";

const mado::Route kRoutes[] PROGMEM = {
    {&kRoutePath0[0], mado::Method::kGet, kGetCard},
    {&kRoutePath1[0], mado::Method::kGet, kGetFormat},
    {&kRoutePath1[0], mado::Method::kPost, kStartFormat},
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
  static constexpr uint8_t kMaxRoutes = 3U;
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 100000UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
  static constexpr uint16_t kMaxDrainBytes = 1024U;
  static constexpr uint16_t kFileChunkSize = 32U;
};

inline bool route_needs_token(const uint8_t route) {
  const bool known = mado::check(route <= kStartFormat);
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
  const bool valid = mado::check(path != nullptr) && mado::check(route <= kStartFormat);
  if (!valid) {
    return false;
  }
  switch (route) {
    default:
      return false;
  }
}

struct Card {
  int32_t capacity_mib;
  char kind[5];
  char file_system[6];
};

constexpr uint8_t kCardCapacityMib = 0x01U;
constexpr uint8_t kCardKind = 0x02U;
constexpr uint8_t kCardFileSystem = 0x04U;
constexpr uint8_t kCardRequired = 0x07U;
constexpr uint8_t kCardWritable = 0x07U;
constexpr uint8_t kCardKeySize = 13U;
constexpr uint8_t kCardFieldCount = 3U;

template <typename ExchangeT>
bool write_card(ExchangeT& exchange, const Card& value) {
  const bool sound = mado::check((value.kind[sizeof(value.kind) - 1U] == '\0') && (value.file_system[sizeof(value.file_system) - 1U] == '\0') && (value.capacity_mib >= 0) && (value.capacity_mib <= 2097152));
  bool written = sound;
  written = written && (exchange.print(F("{\"capacity_mib\":")) > 0U);
  written = written && (exchange.print_signed(value.capacity_mib) > 0U);
  written = written && (exchange.print(F(",\"kind\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.kind[0]);
  written = written && (exchange.print(F(",\"file_system\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.file_system[0]);
  return written && (exchange.print(F("}")) > 0U);
}

struct FormatRequest {
  int32_t capacity_mib;
};

constexpr uint8_t kFormatRequestCapacityMib = 0x01U;
constexpr uint8_t kFormatRequestRequired = 0x01U;
constexpr uint8_t kFormatRequestWritable = 0x01U;
constexpr uint8_t kFormatRequestKeySize = 13U;
constexpr uint8_t kFormatRequestFieldCount = 1U;

template <typename SourceT>
mado::JsonStatus read_format_request_capacity_mib(mado::JsonReader<SourceT>& reader, FormatRequest& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kFormatRequestCapacityMib) == 0U);
  present = static_cast<uint8_t>(present | kFormatRequestCapacityMib);
  if (!fresh) {
    return mado::JsonStatus::kUnsupported;
  }
  int32_t number = 0;
  const mado::JsonStatus status = reader.read_integer(number);
  const bool in_range = mado::check((number >= 0) && (number <= 2097152));
  value.capacity_mib = in_range ? number : value.capacity_mib;
  const bool rejected = (status == mado::JsonStatus::kOk) && (!in_range);
  return rejected ? mado::JsonStatus::kOutOfRange : status;
}

template <typename SourceT>
mado::JsonStatus read_format_request_member(mado::JsonReader<SourceT>& reader, const char* const key,
                                    FormatRequest& value, uint8_t& present) {
  const bool valid = mado::check(key != nullptr) && mado::check((present & static_cast<uint8_t>(~kFormatRequestWritable)) == 0U);
  if (!valid) {
    return mado::JsonStatus::kSyntax;
  }
  if (strcmp_P(key, PSTR("capacity_mib")) == 0) {
    return read_format_request_capacity_mib(reader, value, present);
  }
  return mado::JsonStatus::kUnsupported;
}

template <typename SourceT>
mado::JsonStatus read_format_request(mado::JsonReader<SourceT>& reader, FormatRequest& value,
                                uint8_t& present) {
  present = 0U;
  mado::JsonStatus status = reader.begin_object();
  for (uint8_t i = 0U; (status == mado::JsonStatus::kOk) && (i <= kFormatRequestFieldCount); ++i) {
    char key[kFormatRequestKeySize] = {};
    status = reader.next_key(&key[0], sizeof(key));
    if (status == mado::JsonStatus::kEnd) {
      return reader.finish();
    }
    if (status == mado::JsonStatus::kOk) {
      status = read_format_request_member(reader, &key[0], value, present);
    }
  }
  const bool failed = mado::check(status != mado::JsonStatus::kEnd);
  return (failed && (status != mado::JsonStatus::kOk)) ? status : mado::JsonStatus::kUnsupported;
}

inline mado::Problem format_request_missing(const uint8_t present) {
  const bool sound = mado::check((present & static_cast<uint8_t>(~kFormatRequestWritable)) == 0U);
  if (!sound) {
    return mado::Problem{mado::http::kUnprocessableContent, nullptr};
  }
  if ((present & kFormatRequestCapacityMib) == 0U) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: capacity_mib")};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_format_request_body(ExchangeT& exchange, FormatRequest& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (framing.status != mado::http::kNoResponse) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_format_request(reader, value, present);
  if (status != mado::JsonStatus::kOk) {
    return mado::json_problem(status);
  }
  return format_request_missing(present);
}

struct FormatStatus {
  char phase[11];
  int32_t percent;
  int32_t capacity_mib;
  char file_system[6];
  char detail[65];
};

constexpr uint8_t kFormatStatusPhase = 0x01U;
constexpr uint8_t kFormatStatusPercent = 0x02U;
constexpr uint8_t kFormatStatusCapacityMib = 0x04U;
constexpr uint8_t kFormatStatusFileSystem = 0x08U;
constexpr uint8_t kFormatStatusDetail = 0x10U;
constexpr uint8_t kFormatStatusRequired = 0x1FU;
constexpr uint8_t kFormatStatusWritable = 0x1FU;
constexpr uint8_t kFormatStatusKeySize = 13U;
constexpr uint8_t kFormatStatusFieldCount = 5U;

template <typename ExchangeT>
bool write_format_status(ExchangeT& exchange, const FormatStatus& value) {
  const bool sound = mado::check((value.phase[sizeof(value.phase) - 1U] == '\0') && (value.file_system[sizeof(value.file_system) - 1U] == '\0') && (value.detail[sizeof(value.detail) - 1U] == '\0') && (value.percent >= 0) && (value.percent <= 100) && (value.capacity_mib >= 0) && (value.capacity_mib <= 2097152));
  bool written = sound;
  written = written && (exchange.print(F("{\"phase\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.phase[0]);
  written = written && (exchange.print(F(",\"percent\":")) > 0U);
  written = written && (exchange.print_signed(value.percent) > 0U);
  written = written && (exchange.print(F(",\"capacity_mib\":")) > 0U);
  written = written && (exchange.print_signed(value.capacity_mib) > 0U);
  written = written && (exchange.print(F(",\"file_system\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.file_system[0]);
  written = written && (exchange.print(F(",\"detail\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.detail[0]);
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

}

#endif
