#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeClient.h"
#include "FakeData.h"
#include "mado/FlashText.h"
#include "mado/Method.h"
#include "mado/http_status.h"

namespace {

using Harness = ExchangeHarness;
using TestExchange = HarnessExchange;

TEST(Exchange, ExposesTheRequestMethod) {
  // Arrange
  Harness harness("", 0U, mado::Method::kPatch);
  const TestExchange exchange = harness.make();

  // Act
  const mado::Method method = exchange.method();

  // Assert
  EXPECT_EQ(method, mado::Method::kPatch);
}

TEST(Exchange, ExposesTheDeclaredContentLength) {
  // Arrange
  FakeData data(3U);
  const uint32_t length = data.number(1U, 100000U);
  Harness harness("", length);
  const TestExchange exchange = harness.make();

  // Act
  const uint32_t reported = exchange.content_length();

  // Assert
  EXPECT_EQ(reported, length);
}

TEST(Exchange, ExposesTheUnderlyingClient) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const FakeClient* client = exchange.client();

  // Assert
  EXPECT_EQ(client, &harness.client());
}

TEST(Exchange, RedirectsWithSeeOther) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_redirect(F("/done"));

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 303 See Other\r\nConnection: close\r\nLocation: /done\r\n"
            "Content-Length: 0\r\n\r\n");
}

TEST(Exchange, RefusesARedirectAfterTheStatus) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_redirect(F("/done"));

  // Assert
  EXPECT_FALSE(sent);
}

struct SignedCase {
  int32_t value;
  const char* expected;
};

class PrintSignedTest : public ::testing::TestWithParam<SignedCase> {};

TEST_P(PrintSignedTest, PrintsTheValueWithItsSign) {
  // Arrange
  const SignedCase sample = GetParam();
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const size_t written = exchange.print_signed(sample.value);

  // Assert
  EXPECT_EQ(harness.sent(), sample.expected);
  EXPECT_EQ(written, strlen(sample.expected));
}

INSTANTIATE_TEST_SUITE_P(Values, PrintSignedTest,
                         ::testing::Values(SignedCase{0, "0"}, SignedCase{-5, "-5"},
                                           SignedCase{2147483647, "2147483647"},
                                           SignedCase{-2147483647 - 1, "-2147483648"}));

TEST(Exchange, RefusesASecondStatusLine) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_status(mado::http::kNotFound);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(Exchange, RefusesAnInvalidStatusCode) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_status(99U);

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_FALSE(exchange.status_sent());
}

TEST(Exchange, ReportsThatTheStatusWasSent) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.status_sent();

  // Assert
  EXPECT_TRUE(sent);
}

TEST(Exchange, RefusesHeadersBeforeTheStatus) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_header(F("X-A"), "b");

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(Exchange, RefusesAMissingHeaderValue) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_header(F("X-A"), static_cast<const char*>(nullptr));

  // Assert
  EXPECT_FALSE(sent);
}

TEST(Exchange, RefusesAMissingFlashHeaderValue) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_header(
      F("X-A"), mado::FlashText(static_cast<const __FlashStringHelper*>(nullptr)));

  // Assert
  EXPECT_FALSE(sent);
}

TEST(Exchange, RefusesANumericHeaderBeforeTheStatus) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_content_length(3U);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(Exchange, RefusesEndingHeadersTwice) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);
  exchange.end_headers();

  // Act
  const bool ended = exchange.end_headers();

  // Assert
  EXPECT_FALSE(ended);
}

TEST(Exchange, RefusesAnEmptyResponseAfterTheStatus) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_empty(mado::http::kNoContent);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(Exchange, IgnoresAMissingText) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const size_t written = exchange.print(static_cast<const char*>(nullptr));

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(Exchange, IgnoresAMissingFlashText) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const size_t written = exchange.print(static_cast<const __FlashStringHelper*>(nullptr));

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(Exchange, WritesSingleBytes) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();

  // Act
  const size_t written = exchange.write(static_cast<uint8_t>('z'));

  // Assert
  EXPECT_EQ(written, 1U);
  EXPECT_EQ(harness.sent(), "z");
}

TEST(Exchange, FindsACapturedHeaderIgnoringCase) {
  // Arrange
  static const char* const kNames[] = {"Host", nullptr};
  FakeData data(9U);
  const std::string host = data.word(10);
  Harness harness;
  harness.capture(&kNames[0], 0U, host);
  const TestExchange exchange = harness.make();

  // Act
  const char* value = exchange.header("hOST");

  // Assert
  EXPECT_EQ(std::string(value), host);
}

TEST(Exchange, FindsNoHeaderWithoutRequestedNames) {
  // Arrange
  Harness harness;
  const TestExchange exchange = harness.make();

  // Act
  const char* value = exchange.header("Host");

  // Assert
  EXPECT_EQ(value, nullptr);
}

TEST(Exchange, RejectsAQueryParameterWithoutAName) {
  // Arrange
  Harness harness;
  const TestExchange exchange = harness.make();
  char out[8] = {};

  // Act
  const bool found = exchange.query_param(static_cast<const char*>(nullptr), &out[0], sizeof(out));

  // Assert
  EXPECT_FALSE(found);
  EXPECT_FALSE(
      exchange.query_param(static_cast<const __FlashStringHelper*>(nullptr), &out[0], sizeof(out)));
  EXPECT_EQ(exchange.header(static_cast<const __FlashStringHelper*>(nullptr)), nullptr);
}

TEST(Exchange, RejectsABodyReadIntoNothing) {
  // Arrange
  Harness harness("abc", 3U);
  TestExchange exchange = harness.make();

  // Act
  const int16_t got = exchange.read_body(nullptr, 4U);

  // Assert
  EXPECT_EQ(got, -1);
}

TEST(Exchange, ReadsNothingWhenThereIsNoBody) {
  // Arrange
  Harness harness;
  TestExchange exchange = harness.make();
  uint8_t buffer[4] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sizeof(buffer));

  // Assert
  EXPECT_EQ(got, 0);
}

TEST(Exchange, ReportsAClosedConnectionDuringABodyRead) {
  // Arrange
  Harness harness("", 3U);
  harness.client().disconnect();
  TestExchange exchange = harness.make();
  uint8_t buffer[4] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sizeof(buffer));

  // Assert
  EXPECT_EQ(got, -1);
}

TEST(Exchange, NeverReadsPastTheDeclaredLength) {
  // Arrange
  Harness harness("abcdef", 2U);
  TestExchange exchange = harness.make();
  uint8_t buffer[8] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sizeof(buffer));

  // Assert
  EXPECT_EQ(got, 2);
  EXPECT_EQ(exchange.remaining_body(), 0U);
}

}
