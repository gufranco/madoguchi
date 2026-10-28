#include <Arduino.h>
#include <ArduinoJson.h>
#include <MadoguchiJson.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeData.h"
#include "FakeFile.h"
#include "mado/FileSource.h"
#include "mado/MemorySource.h"
#include "mado/Method.h"
#include "mado/check_basic_auth.h"
#include "mado/http_status.h"
#include "mado/send_auth_challenge.h"
#include "mado/send_template.h"

namespace {

struct AuthCase {
  const char* authorization;
  bool expected;
};

class CheckBasicAuthTest : public ::testing::TestWithParam<AuthCase> {};

TEST_P(CheckBasicAuthTest, AcceptsOnlyTheExactCredential) {
  // Arrange
  const AuthCase sample = GetParam();

  // Act
  const bool allowed = mado::check_basic_auth(sample.authorization, F("dXNlcjpwYXNz"));

  // Assert
  EXPECT_EQ(allowed, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Headers, CheckBasicAuthTest,
    ::testing::Values(AuthCase{"Basic dXNlcjpwYXNz", true}, AuthCase{"basic dXNlcjpwYXNz", true},
                      AuthCase{"Basic dXNlcjpwYXNy", false}, AuthCase{"Basic dXNlcjpwYXN", false},
                      AuthCase{"Basic dXNlcjpwYXNzz", false},
                      AuthCase{"Bearer dXNlcjpwYXNz", false}, AuthCase{"Basic", false},
                      AuthCase{"", false}, AuthCase{nullptr, false}));

TEST(CheckBasicAuth, RejectsAMissingExpectedCredential) {
  // Arrange
  const char* authorization = "Basic dXNlcjpwYXNz";

  // Act
  const bool allowed =
      mado::check_basic_auth(authorization, static_cast<const __FlashStringHelper*>(nullptr));

  // Assert
  EXPECT_FALSE(allowed);
}

TEST(SendAuthChallenge, AsksForBasicCredentials) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_auth_challenge(exchange, F("Lights"));

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n"
            "WWW-Authenticate: Basic realm=\"Lights\", charset=\"UTF-8\"\r\n"
            "Content-Length: 0\r\n\r\n");
}

TEST(SendAuthChallenge, RefusesAfterTheStatusWasSent) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = mado::send_auth_challenge(exchange, F("Lights"));

  // Assert
  EXPECT_FALSE(sent);
}

struct TestTags {
  static bool render(HarnessExchange& exchange, const char* tag) {
    const std::string name(tag);
    if (name == "TEMP") {
      exchange.print(static_cast<uint32_t>(21U));
      return true;
    }
    if (name == "EMPTY") {
      return true;
    }
    return false;
  }
};

struct TemplateCase {
  const char* page;
  const char* expected;
};

class SendTemplateTest : public ::testing::TestWithParam<TemplateCase> {};

TEST_P(SendTemplateTest, ReplacesKnownTagsAndKeepsEverythingElse) {
  // Arrange
  const TemplateCase sample = GetParam();
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  mado::MemorySource source(reinterpret_cast<const uint8_t*>(sample.page),
                            static_cast<uint16_t>(strlen(sample.page)));

  // Act
  const bool sent = mado::send_template<HarnessExchange, TestTags>(exchange, source);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Pages, SendTemplateTest,
    ::testing::Values(
        TemplateCase{"<p>$TEMP$</p>", "<p>21</p>"}, TemplateCase{"$TEMP$C $EMPTY$!", "21C !"},
        TemplateCase{"<p>$X$</p>", "<p>$X$</p>"}, TemplateCase{"cost: 5$", "cost: 5$"},
        TemplateCase{"$$", "$$"}, TemplateCase{"$$TEMP$", "$21"}, TemplateCase{"$a-b$", "$a-b$"},
        TemplateCase{"$ABCDEFGHIJKLMNOPQ$", "$ABCDEFGHIJKLMNOPQ$"}, TemplateCase{"", ""}));

TEST(SendTemplate, ReadsTheTemplateFromAFile) {
  // Arrange
  FakeFile file("<b>$TEMP$</b>");
  mado::FileSource<FakeFile> source(file);
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_template<HarnessExchange, TestTags>(exchange, source);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(), "<b>21</b>");
}

TEST(SendTemplate, StopsWhenTheClientStopsAccepting) {
  // Arrange
  FakeData data(51U);
  const std::string page = data.word(100);
  ExchangeHarness harness;
  harness.client().limit_write(1U);
  HarnessExchange exchange = harness.make();
  mado::MemorySource source(reinterpret_cast<const uint8_t*>(page.data()),
                            static_cast<uint16_t>(page.size()));

  // Act
  const bool sent = mado::send_template<HarnessExchange, TestTags>(exchange, source);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(SendJson, SendsTheDocumentWithAnExactLength) {
  // Arrange
  const JsonDocument document;
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_json(exchange, mado::http::kOk, document);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n"
            "Content-Length: 4\r\n\r\nnull");
}

TEST(SendJson, SendsOnlyHeadersForHead) {
  // Arrange
  const JsonDocument document;
  ExchangeHarness harness("", 0U, mado::Method::kHead);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_json(exchange, mado::http::kOk, document);

  // Assert
  EXPECT_TRUE(sent);
  EXPECT_EQ(harness.sent(),
            "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n"
            "Content-Length: 4\r\n\r\n");
}

TEST(SendJson, RefusesAfterTheStatusWasSent) {
  // Arrange
  const JsonDocument document;
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  exchange.send_status(mado::http::kOk);

  // Act
  const bool sent = mado::send_json(exchange, mado::http::kOk, document);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(SendJson, ReportsATruncatedBody) {
  // Arrange
  const JsonDocument document;
  ExchangeHarness harness;
  harness.client().limit_write(1U);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_json(exchange, mado::http::kOk, document);

  // Assert
  EXPECT_FALSE(sent);
}

}
