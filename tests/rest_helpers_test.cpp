#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "ExchangeHarness.h"
#include "mado/JsonStatus.h"
#include "mado/Method.h"
#include "mado/Route.h"
#include "mado/SlotTable.h"
#include "mado/TokenStatus.h"
#include "mado/is_json_media_type.h"
#include "mado/json_error_status.h"
#include "mado/parse_path_id.h"
#include "mado/send_bearer_challenge.h"
#include "mado/send_method_not_allowed.h"

namespace {

struct Sample {
  int32_t value;
};

const mado::Route kRoutes[] = {
    {"/api/items", mado::Method::kGet, 0U},     {"/api/items", mado::Method::kPost, 1U},
    {"/api/items/*", mado::Method::kGet, 2U},   {"/api/items/*", mado::Method::kPut, 3U},
    {"/api/items/*", mado::Method::kPatch, 4U}, {"/api/items/*", mado::Method::kDelete, 5U},
    {"/api/login", mado::Method::kPost, 6U},    {"/api/ping", mado::Method::kOptions, 7U},
    {nullptr, mado::Method::kAny, 0U},
};

std::string allow_for(const char* path) {
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  static_cast<void>(mado::send_method_not_allowed<16U>(exchange, &kRoutes[0], path));
  const std::string sent = harness.sent();
  const size_t start = sent.find("Allow: ");
  return (start == std::string::npos)
             ? "<none>"
             : sent.substr(start + 7U, sent.find('\r', start) - start - 7U);
}

class AllowHeaderTest : public ::testing::TestWithParam<std::pair<const char*, const char*>> {};

TEST_P(AllowHeaderTest, ListsEveryMethodForThePath) {
  // Arrange
  const std::pair<const char*, const char*>& sample = GetParam();

  // Act
  const std::string allow = allow_for(sample.first);

  // Assert
  EXPECT_EQ(allow, sample.second);
}

INSTANTIATE_TEST_SUITE_P(
    Paths, AllowHeaderTest,
    ::testing::Values(std::make_pair("/api/items", "GET, HEAD, POST"),
                      std::make_pair("/api/items/3", "GET, HEAD, PUT, DELETE, PATCH"),
                      std::make_pair("/api/login", "POST"), std::make_pair("/api/ping", "OPTIONS"),
                      std::make_pair("/nothing", "")));

TEST(SendMethodNotAllowed, AnswersWithAnEmptyBody) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_method_not_allowed<16U>(exchange, &kRoutes[0], "/api/login");

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\nAllow: POST\r\n"
            "Content-Length: 0\r\n\r\n");
}

TEST(SendMethodNotAllowed, StopsAtTheRouteLimit) {
  // Arrange
  const char* const path = "/api/login";

  // Act
  const uint16_t mask = mado::detail::allowed_methods<2U>(&kRoutes[0], path);

  // Assert
  EXPECT_EQ(mask, 0U);
}

struct ChallengeCase {
  mado::TokenStatus status;
  const char* expected;
};

class BearerChallengeTest : public ::testing::TestWithParam<ChallengeCase> {};

TEST_P(BearerChallengeTest, AnswersEachRefusal) {
  // Arrange
  const ChallengeCase& sample = GetParam();
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_bearer_challenge(exchange, sample.status);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Statuses, BearerChallengeTest,
    ::testing::Values(
        ChallengeCase{mado::TokenStatus::kMissing,
                      "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n"
                      "WWW-Authenticate: Bearer realm=\"api\"\r\nContent-Length: 0\r\n\r\n"},
        ChallengeCase{mado::TokenStatus::kExpired,
                      "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n"
                      "WWW-Authenticate: Bearer realm=\"api\", error=\"invalid_token\"\r\n"
                      "Content-Length: 0\r\n\r\n"},
        ChallengeCase{mado::TokenStatus::kUnprovisioned,
                      "HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n"
                      "Content-Length: 0\r\n\r\n"}));

TEST(SendBearerChallenge, RefusesAValidToken) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_bearer_challenge(exchange, mado::TokenStatus::kValid);

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

struct IdCase {
  const char* path;
  bool valid;
  uint8_t id;
};

class ParsePathIdTest : public ::testing::TestWithParam<IdCase> {};

TEST_P(ParsePathIdTest, ReadsCanonicalIds) {
  // Arrange
  const IdCase& sample = GetParam();
  uint8_t item_id = 99U;

  // Act
  const bool parsed = mado::parse_path_id(sample.path, PSTR("/api/items/"), item_id);

  // Assert
  EXPECT_EQ(parsed, sample.valid);
  EXPECT_EQ(item_id, sample.id);
}

INSTANTIATE_TEST_SUITE_P(
    Paths, ParsePathIdTest,
    ::testing::Values(IdCase{"/api/items/1", true, 1U}, IdCase{"/api/items/254", true, 254U},
                      IdCase{"/api/items/255", false, 0U}, IdCase{"/api/items/0", false, 0U},
                      IdCase{"/api/items/01", false, 0U}, IdCase{"/api/items/1000", false, 0U},
                      IdCase{"/api/items/", false, 0U}, IdCase{"/api/items/7x", false, 0U},
                      IdCase{"/api/items/7/", false, 0U}, IdCase{"/api/item/7", false, 0U},
                      IdCase{nullptr, false, 0U}));

TEST(SlotTable, HandsOutTheLowestFreeId) {
  // Arrange
  mado::SlotTable<Sample, 3U> table;
  static_cast<void>(table.insert(Sample{1}));
  static_cast<void>(table.insert(Sample{2}));
  static_cast<void>(table.erase(1U));

  // Act
  const uint8_t slot_id = table.insert(Sample{3});

  // Assert
  EXPECT_EQ(slot_id, 1U);
  EXPECT_EQ(table.find(1U)->value, 3);
  EXPECT_EQ(table.count(), 2U);
}

TEST(SlotTable, ReportsAFullTable) {
  // Arrange
  mado::SlotTable<Sample, 2U> table;
  static_cast<void>(table.insert(Sample{1}));
  static_cast<void>(table.insert(Sample{2}));

  // Act
  const uint8_t slot_id = table.insert(Sample{3});

  // Assert
  EXPECT_EQ(slot_id, 0U);
  EXPECT_EQ(table.count(), (mado::SlotTable<Sample, 2U>::capacity()));
}

TEST(SlotTable, FindsNothingOutsideTheTable) {
  // Arrange
  const mado::SlotTable<Sample, 2U> table;

  // Act
  const Sample* found = table.find(3U);

  // Assert
  EXPECT_EQ(found, nullptr);
  EXPECT_EQ(table.find(0U), nullptr);
}

TEST(SlotTable, RefusesToEraseAnEmptySlot) {
  // Arrange
  mado::SlotTable<Sample, 2U> table;

  // Act
  const bool erased = table.erase(1U);

  // Assert
  EXPECT_FALSE(erased);
}

class JsonMediaTypeTest : public ::testing::TestWithParam<std::pair<const char*, bool>> {};

TEST_P(JsonMediaTypeTest, AcceptsJsonWithParameters) {
  // Arrange
  const std::pair<const char*, bool>& sample = GetParam();

  // Act
  const bool json = mado::is_json_media_type(sample.first);

  // Assert
  EXPECT_EQ(json, sample.second);
}

INSTANTIATE_TEST_SUITE_P(
    Types, JsonMediaTypeTest,
    ::testing::Values(std::make_pair("application/json", true),
                      std::make_pair("Application/JSON; charset=utf-8", true),
                      std::make_pair("application/json ", true),
                      std::make_pair("application/jsonp", false),
                      std::make_pair("application/js", false), std::make_pair("text/plain", false),
                      std::make_pair(static_cast<const char*>(nullptr), false)));

struct ErrorCase {
  mado::JsonStatus status = mado::JsonStatus::kOk;
  uint16_t expected = 0U;
};

class JsonErrorStatusTest : public ::testing::TestWithParam<ErrorCase> {};

TEST_P(JsonErrorStatusTest, MapsReaderFailuresToHttp) {
  // Arrange
  const ErrorCase& sample = GetParam();

  // Act
  const uint16_t code = mado::json_error_status(sample.status);

  // Assert
  EXPECT_EQ(code, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Statuses, JsonErrorStatusTest,
                         ::testing::Values(ErrorCase{mado::JsonStatus::kSyntax, 400U},
                                           ErrorCase{mado::JsonStatus::kTruncated, 400U},
                                           ErrorCase{mado::JsonStatus::kTooLong, 422U},
                                           ErrorCase{mado::JsonStatus::kOutOfRange, 422U},
                                           ErrorCase{mado::JsonStatus::kWrongType, 422U},
                                           ErrorCase{mado::JsonStatus::kUnsupported, 422U},
                                           ErrorCase{mado::JsonStatus::kOk, 500U},
                                           ErrorCase{mado::JsonStatus::kEnd, 500U}));

}
