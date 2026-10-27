#include <Arduino.h>
#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "TestApp.h"
#include "mado/FlashText.h"
#include "mado/HttpVersion.h"
#include "mado/Method.h"
#include "mado/RequestLine.h"
#include "mado/http_status.h"

namespace {

std::string request_text(const std::string& line, const std::string& headers = "") {
  return line + "\r\nHost: mado.test\r\n" + headers + "\r\n";
}

TEST(ServerDefense, IgnoresClientsWhenNoRoutesWereGiven) {
  // Arrange
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(nullptr, nullptr);
  FakeClient client(request_text("GET / HTTP/1.1"));

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.output(), "");
  EXPECT_FALSE(client.stopped());
}

TEST(ServerDefense, ClosesWhenTheClientStopsAcceptingOutput) {
  // Arrange
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request_text("GET /stream HTTP/1.1"));
  client.limit_write(1U);

  // Act
  server.process(client);

  // Assert
  EXPECT_TRUE(client.stopped());
}

TEST(ServerDefense, AnswersNothingWhenTheClientHangsUpMidRequest) {
  // Arrange
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client("GET / HTTP/1.1\r\nHost: a");
  client.close_when_drained();

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.output(), "");
  EXPECT_TRUE(client.stopped());
}

TEST(ServerDefense, ReportsADriverThatReturnsAnImpossibleByte) {
  // Arrange
  fake_clock::set(0U);
  mado::Server<FakeClient, TestApp> server(test_routes(), test_header_names());
  FakeClient client(request_text("GET / HTTP/1.1"));
  client.force_single_read(300);

  // Act
  server.process(client);

  // Assert
  EXPECT_EQ(client.output(), "");
  EXPECT_TRUE(client.stopped());
}

TEST(ExchangeDefense, ReplacesAMissingPathAndQuery) {
  // Arrange
  ExchangeHarness harness;
  harness.set_line(
      mado::RequestLine{mado::Method::kGet, nullptr, nullptr, true, mado::HttpVersion::kHttp11});
  const HarnessExchange exchange = harness.make();

  // Act
  const std::string path(exchange.path());

  // Assert
  EXPECT_EQ(path, "/");
  EXPECT_EQ(std::string(exchange.query()), "");
}

TEST(ExchangeDefense, RejectsAZeroSizedQueryBuffer) {
  // Arrange
  ExchangeHarness harness;
  const HarnessExchange exchange = harness.make();
  char out[4] = {};

  // Act
  const bool found = exchange.query_param("q", &out[0], 0U);

  // Assert
  EXPECT_FALSE(found);
}

struct BodySizeCase {
  uint16_t size;
};

class ReadBodySizeTest : public ::testing::TestWithParam<BodySizeCase> {};

TEST_P(ReadBodySizeTest, RejectsBufferSizesOutsideTheChunkRange) {
  // Arrange
  const BodySizeCase sample = GetParam();
  ExchangeHarness harness("abc", 3U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  uint8_t buffer[4] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sample.size);

  // Assert
  EXPECT_EQ(got, -1);
}

INSTANTIATE_TEST_SUITE_P(Sizes, ReadBodySizeTest,
                         ::testing::Values(BodySizeCase{0U}, BodySizeCase{0x8000U}));

TEST(ExchangeDefense, RejectsADriverThatClaimsMoreBytesThanAsked) {
  // Arrange
  ExchangeHarness harness("abc", 3U, mado::Method::kPost);
  harness.client().overstate_block_reads(5);
  HarnessExchange exchange = harness.make();
  uint8_t buffer[4] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sizeof(buffer));

  // Assert
  EXPECT_EQ(got, -1);
}

TEST(ExchangeDefense, SendsAnEmptyHeaderValue) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_header(F("X-Empty"), "");

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_NE(harness.sent().find("X-Empty: \r\n"), std::string::npos);
}

TEST(ExchangeDefense, RefusesAMissingContentType) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_content_type(nullptr);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(ExchangeDefense, RefusesCorsHeadersBeforeTheStatus) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_cors_headers();

  // Assert
  EXPECT_FALSE(sent);
}

TEST(ExchangeDefense, RefusesEndingHeadersBeforeTheStatus) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool ended = exchange.end_headers();

  // Assert
  EXPECT_FALSE(ended);
}

TEST(ExchangeDefense, RefusesAnEmptyResponseWithAnInvalidCode) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_empty(600U);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(ExchangeDefense, RefusesARedirectWithoutALocation) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = exchange.send_redirect(nullptr);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(ExchangeDefense, RefusesMissingBytesWithALength) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const size_t written = exchange.write(nullptr, 3U);

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(ExchangeDefense, WritesNothingOnceTheOutputIsDetached) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  harness.detach_output();

  // Act
  const size_t written = exchange.print(F("lost"));

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(ExchangeDefense, StillReadsTheBodyWhenTheContinueCannotBeSent) {
  // Arrange
  FakeData data(61U);
  const std::string body = data.word(3);
  ExchangeHarness harness(body, 3U, mado::Method::kPut);
  harness.client().limit_write(0U);
  HarnessExchange exchange = harness.make();
  uint8_t buffer[8] = {};

  // Act
  const int16_t got = exchange.read_body(&buffer[0], sizeof(buffer));

  // Assert
  EXPECT_EQ(got, 3);
}

TEST(ExchangeDefense, FindsNoHeaderThatWasNotRequested) {
  // Arrange
  static const char* const kNames[] = {"Host", nullptr};
  ExchangeHarness harness;
  harness.capture(&kNames[0], 0U, "example");
  const HarnessExchange exchange = harness.make();

  // Act
  const char* value = exchange.header("Accept");

  // Assert
  EXPECT_EQ(value, nullptr);
}

TEST(ExchangeDefense, RefusesAHeaderNameThatIsMissing) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = exchange.send_header(nullptr, mado::FlashText(F("v")));

  // Assert
  EXPECT_FALSE(sent);
}

}
