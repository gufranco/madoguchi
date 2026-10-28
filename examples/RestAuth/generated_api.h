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

namespace rest_auth {

enum RouteId : uint8_t {
  kLogin,
  kGetGreeting,
  kOpenApiDocument,
  kCardFile,
};

const char kRoutePath0[] PROGMEM = "/api/openapi.json";
const char kRoutePath1[] PROGMEM = "/api/auth/login";
const char kRoutePath2[] PROGMEM = "/api/greeting";
const char kRoutePath3[] PROGMEM = "/*";

const mado::Route kRoutes[] PROGMEM = {
    {&kRoutePath0[0], mado::Method::kGet, kOpenApiDocument},
    {&kRoutePath1[0], mado::Method::kPost, kLogin},
    {&kRoutePath2[0], mado::Method::kGet, kGetGreeting},
    {&kRoutePath3[0], mado::Method::kGet, kCardFile},
    {nullptr, mado::Method::kAny, 0U},
};

const char kHeaderName0[] PROGMEM = "Authorization";
const char kHeaderName1[] PROGMEM = "Content-Type";
const char kHeaderName2[] PROGMEM = "Accept-Encoding";
const char kHeaderName3[] PROGMEM = "If-None-Match";
const char* const kHeaderNames[] PROGMEM = {&kHeaderName0[0], &kHeaderName1[0], &kHeaderName2[0], &kHeaderName3[0], nullptr};

struct Limits {
  static constexpr uint16_t kLineBufferSize = 128U;
  static constexpr uint16_t kOutputBufferSize = 32U;
  static constexpr uint8_t kHeaderPoolSize = 240U;
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

inline bool route_needs_token(const uint8_t route) {
  const bool known = mado::check(route <= kCardFile);
  if (!known) {
    return true;
  }
  switch (route) {
    case kGetGreeting:
      return true;
    default:
      return false;
  }
}

inline bool route_path_id(const char* const path, const uint8_t route, uint8_t& id) {
  id = 0U;
  const bool valid = mado::check(path != nullptr) && mado::check(route <= kCardFile);
  if (!valid) {
    return false;
  }
  switch (route) {
    default:
      return false;
  }
}

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
  present = static_cast<uint8_t>(present | kCredentialsUsername);
  if (!fresh) {
    return mado::JsonStatus::kUnsupported;
  }
  return reader.read_string(&value.username[0], sizeof(value.username));
}

template <typename SourceT>
mado::JsonStatus read_credentials_password(mado::JsonReader<SourceT>& reader, Credentials& value,
                                    uint8_t& present) {
  const bool fresh = mado::check((present & kCredentialsPassword) == 0U);
  present = static_cast<uint8_t>(present | kCredentialsPassword);
  if (!fresh) {
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
  if ((present & kCredentialsUsername) == 0U) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: username")};
  }
  if ((present & kCredentialsPassword) == 0U) {
    return mado::Problem{mado::http::kUnprocessableContent, F("missing required property: password")};
  }
  return mado::Problem{mado::http::kNoResponse, nullptr};
}

template <typename ExchangeT>
mado::Problem read_credentials_body(ExchangeT& exchange, Credentials& value, uint8_t& present,
                                const uint32_t max_body) {
  present = 0U;
  const mado::Problem framing = mado::json_body_problem(exchange, max_body);
  if (framing.status != mado::http::kNoResponse) {
    return framing;
  }
  mado::BodySource<ExchangeT> source(exchange);
  mado::JsonReader<mado::BodySource<ExchangeT>> reader(source);
  const mado::JsonStatus status = read_credentials(reader, value, present);
  if (status != mado::JsonStatus::kOk) {
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
  const bool sound = mado::check((value.access_token[sizeof(value.access_token) - 1U] == '\0') && (value.token_type[sizeof(value.token_type) - 1U] == '\0') && (value.expires_in >= 1) && (value.expires_in <= 900));
  bool written = sound;
  written = written && (exchange.print(F("{\"access_token\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.access_token[0]);
  written = written && (exchange.print(F(",\"token_type\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.token_type[0]);
  written = written && (exchange.print(F(",\"expires_in\":")) > 0U);
  written = written && (exchange.print_signed(value.expires_in) > 0U);
  return written && (exchange.print(F("}")) > 0U);
}

struct Greeting {
  char message[41];
};

constexpr uint8_t kGreetingMessage = 0x01U;
constexpr uint8_t kGreetingRequired = 0x01U;
constexpr uint8_t kGreetingWritable = 0x01U;
constexpr uint8_t kGreetingKeySize = 8U;
constexpr uint8_t kGreetingFieldCount = 1U;

template <typename ExchangeT>
bool write_greeting(ExchangeT& exchange, const Greeting& value) {
  const bool sound = mado::check((value.message[sizeof(value.message) - 1U] == '\0'));
  bool written = sound;
  written = written && (exchange.print(F("{\"message\":")) > 0U);
  written = written && mado::json_write_string(exchange, &value.message[0]);
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
    return mado::send_problem(
        exchange, mado::Problem{mado::http::kServiceUnavailable,
                                F("the SD card is not available")});
  }
  if (route == kOpenApiDocument) {
    return mado::serve_card_path<kReadMode>(exchange, *card, &kOpenApiCardPath[0]);
  }
  return mado::serve_card_file<kReadMode>(exchange, *card);
}

}

#endif
