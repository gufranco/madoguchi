#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <string>

#include "ExchangeHarness.h"
#include "mado/Asset.h"
#include "mado/HttpVersion.h"
#include "mado/Method.h"
#include "mado/RequestLine.h"
#include "mado/etag_matches.h"
#include "mado/send_asset.h"

namespace {

const uint8_t kBody[] PROGMEM = {0x1FU, 0x8BU, 0x08U, 0x00U, 0x41U};

const char* const kNames[] = {"Accept-Encoding", "If-None-Match", nullptr};

mado::Asset html_asset() {
  return mado::Asset{
      &kBody[0],
      sizeof(kBody),
      F("Content-Type: text/html; charset=utf-8\r\nContent-Encoding: gzip\r\nContent-Length: "
        "5\r\nETag: \"0123abcd\"\r\nCache-Control: max-age=300, must-revalidate\r\n"
        "Content-Security-Policy: default-src 'self'\r\n"),
      F("ETag: \"0123abcd\"\r\nCache-Control: max-age=300, must-revalidate\r\n"),
      F("\"0123abcd\""),
  };
}

mado::Asset script_asset() {
  return mado::Asset{
      &kBody[0],
      sizeof(kBody),
      F("Content-Type: text/javascript\r\nCache-Control: public, max-age=31536000, immutable\r\n"),
      F("ETag: \"0123abcd\"\r\n"),
      F("\"0123abcd\""),
  };
}

struct Captured {
  const char* encoding = nullptr;
  const char* tag = nullptr;
};

void capture(ExchangeHarness& harness, const Captured& headers) {
  if (headers.encoding != nullptr) {
    harness.capture(&kNames[0], 0U, headers.encoding);
  }
  if (headers.tag != nullptr) {
    harness.capture(&kNames[0], 1U, headers.tag);
  }
}

TEST(SendAsset, ServesTheGzippedBodyWithSecurityHeaders) {
  // Arrange
  ExchangeHarness harness;
  capture(harness, Captured{"gzip, deflate, br", nullptr});
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, html_asset());

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: 5\r\n"
            "ETag: \"0123abcd\"\r\n"
            "Cache-Control: max-age=300, must-revalidate\r\n"
            "Content-Security-Policy: default-src 'self'\r\n"
            "\r\n" +
                std::string("\x1F\x8B\x08\x00\x41", 5U));
}

TEST(SendAsset, SendsTheHeaderBlockTheGeneratorWroteForAScript) {
  // Arrange
  ExchangeHarness harness;
  capture(harness, Captured{"gzip", nullptr});
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, script_asset());

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent().find("Content-Security-Policy"), std::string::npos);
  EXPECT_NE(harness.sent().find("Cache-Control: public, max-age=31536000, immutable\r\n\r\n"),
            std::string::npos);
}

TEST(SendAsset, AnswersNotModifiedForTheCurrentTag) {
  // Arrange
  ExchangeHarness harness;
  capture(harness, Captured{"gzip", "\"0123abcd\""});
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, html_asset());

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 304 Not Modified\r\nConnection: close\r\n"
            "ETag: \"0123abcd\"\r\n"
            "Cache-Control: max-age=300, must-revalidate\r\n"
            "\r\n");
}

TEST(SendAsset, RefusesClientsWithoutGzip) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, html_asset());

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 406 Not Acceptable\r\nConnection: close\r\n"
            "Vary: Accept-Encoding\r\n"
            "Content-Length: 0\r\n\r\n");
}

TEST(SendAsset, SendsOnlyHeadersForHead) {
  // Arrange
  ExchangeHarness harness;
  harness.set_line(
      mado::RequestLine{mado::Method::kHead, "/", "", true, mado::HttpVersion::kHttp11});
  capture(harness, Captured{"gzip", nullptr});
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, script_asset());

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent().substr(harness.sent().size() - 4U), "\r\n\r\n");
}

TEST(SendAsset, RejectsAnAssetWithoutData) {
  // Arrange
  ExchangeHarness harness;
  capture(harness, Captured{"gzip", nullptr});
  HarnessExchange exchange = harness.make();
  const mado::Asset broken{nullptr, 5U, F("X: y\r\n"), F("X: y\r\n"), F("\"x\"")};

  // Act
  const bool sent = mado::send_asset(exchange, broken);

  // Assert
  EXPECT_FALSE(sent);
  EXPECT_EQ(harness.sent(), "");
}

TEST(SendAsset, ReportsAFailedOutput) {
  // Arrange
  ExchangeHarness harness;
  capture(harness, Captured{"gzip", nullptr});
  harness.client().fail_writes_after(10U);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_asset(exchange, script_asset());

  // Assert
  EXPECT_FALSE(sent);
}

struct TagCase {
  const char* header;
  bool expected;
};

class EtagMatchesTest : public ::testing::TestWithParam<TagCase> {};

TEST_P(EtagMatchesTest, ComparesAgainstEveryListedTag) {
  // Arrange
  const TagCase& sample = GetParam();

  // Act
  const bool matched = mado::etag_matches(sample.header, F("\"0123abcd\""));

  // Assert
  EXPECT_EQ(matched, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Lists, EtagMatchesTest,
                         ::testing::Values(TagCase{"\"0123abcd\"", true},
                                           TagCase{"W/\"0123abcd\"", true},
                                           TagCase{"\"x\", \"0123abcd\"", true},
                                           TagCase{"\"x\",\"0123abcd\"", true}, TagCase{"*", true},
                                           TagCase{"\"0123abcd", false},
                                           TagCase{"\"0123abcde\"", false}, TagCase{"\"x\"", false},
                                           TagCase{"", false}, TagCase{nullptr, false}));

}
