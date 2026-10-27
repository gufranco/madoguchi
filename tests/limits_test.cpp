#include <Arduino.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ExchangeHarness.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeFile.h"
#include "mado/DefaultLimits.h"
#include "mado/FormStatus.h"
#include "mado/FormToken.h"
#include "mado/LineStatus.h"
#include "mado/Method.h"
#include "mado/OutputBuffer.h"
#include "mado/ReadStatus.h"
#include "mado/check_basic_auth.h"
#include "mado/equals_ignore_case_P.h"
#include "mado/next_form_field.h"
#include "mado/read_line.h"
#include "mado/send_file.h"
#include "mado/send_template.h"
#include "mado/wait_for_data.h"

namespace {

struct FewPolls {
  static constexpr uint16_t kReadTimeoutMs = 1000U;
  static constexpr uint32_t kMaxPollsPerRead = 10UL;
  static constexpr uint16_t kRequestTimeoutMs = 5000U;
};

struct NoTags {
  static bool render(const HarnessExchange& exchange, const char* tag) {
    return (exchange.status_sent() && (tag == nullptr));
  }
};

TEST(WaitForData, GivesUpWhenThePollBudgetRunsOut) {
  // Arrange
  fake_clock::set(0U);
  FakeClient client("");
  int32_t available = 0;

  // Act
  const mado::ReadStatus status = mado::wait_for_data<FewPolls>(client, available, 0U);

  // Assert
  EXPECT_EQ(status, mado::ReadStatus::kTimeout);
  EXPECT_EQ(fake_clock::now(), 10U);
}

TEST(ReadLine, RejectsABufferTooSmallForALine) {
  // Arrange
  FakeClient client("GET /\r\n");
  char buffer[1] = {};

  // Act
  const mado::LineStatus status = mado::read_line<mado::DefaultLimits>(client, 0U, &buffer[0], 1U);

  // Assert
  EXPECT_EQ(status, mado::LineStatus::kTooLong);
}

TEST(ReadLine, StopsAfterTooManyCarriageReturns) {
  // Arrange
  FakeClient client("\r\r\r\r\r\r\r\r");
  char buffer[4] = {};

  // Act
  const mado::LineStatus status = mado::read_line<mado::DefaultLimits>(client, 0U, &buffer[0], 4U);

  // Assert
  EXPECT_EQ(status, mado::LineStatus::kTooLong);
  EXPECT_STREQ(&buffer[0], "");
}

TEST(OutputBufferLimits, DropsPendingBytesWhenDetachedEarly) {
  // Arrange
  FakeClient client("");
  mado::OutputBuffer<FakeClient, mado::DefaultLimits> buffer;
  buffer.attach(client);
  buffer.write_byte(static_cast<uint8_t>('x'));
  buffer.detach();
  buffer.attach(client);

  // Act
  const bool flushed = buffer.flush();

  // Assert
  EXPECT_TRUE(flushed);
  EXPECT_EQ(client.output(), "");
}

TEST(OutputBufferLimits, RefusesDiscardModeWhileDetached) {
  // Arrange
  mado::OutputBuffer<FakeClient, mado::DefaultLimits> buffer;

  // Act
  buffer.set_discard(true);

  // Assert
  EXPECT_FALSE(buffer.flush());
}

TEST(OutputBufferLimits, RefusesFlashTextLongerThanTheScanBound) {
  // Arrange
  FakeData data(71U);
  const std::string text = data.word(1100);
  FakeClient client("");
  mado::OutputBuffer<FakeClient, mado::DefaultLimits> buffer;
  buffer.attach(client);

  // Act
  const size_t written = buffer.write_flash(text.c_str());

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(EqualsIgnoreCasePLimits, RejectsFlashTextLongerThanTheScanBound) {
  // Arrange
  FakeData data(72U);
  const std::string text = data.word(1100);

  // Act
  const bool equal = mado::equals_ignore_case_P(text.c_str(), text.c_str());

  // Assert
  EXPECT_FALSE(equal);
}

TEST(CheckBasicAuthLimits, RejectsAMissingSchemeValue) {
  // Arrange
  const char* value = nullptr;

  // Act
  const bool basic = mado::detail::has_basic_scheme(value);

  // Assert
  EXPECT_FALSE(basic);
}

TEST(CheckBasicAuthLimits, TreatsAMissingExpectationAsAMismatch) {
  // Arrange
  const char* given = "abc";

  // Act
  const uint8_t difference = mado::detail::constant_time_difference(given, 3U, nullptr, 3U);

  // Assert
  EXPECT_EQ(difference, 1U);
}

TEST(CheckBasicAuthLimits, RejectsAnExpectedCredentialLongerThanTheScanBound) {
  // Arrange
  FakeData data(73U);
  const std::string expected = data.word(1100);
  const std::string header = "Basic " + expected;

  // Act
  const bool allowed = mado::check_basic_auth(
      header.c_str(), reinterpret_cast<const __FlashStringHelper*>(expected.c_str()));

  // Assert
  EXPECT_FALSE(allowed);
}

TEST(FormLimits, ReportsAnEscapeCutShortByTheBody) {
  // Arrange
  ExchangeHarness harness("n=%", 3U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  char name[8] = {};
  char value[8] = {};

  // Act
  const mado::FormStatus status =
      mado::next_form_field(exchange, &name[0], sizeof(name), &value[0], sizeof(value));

  // Assert
  EXPECT_EQ(status, mado::FormStatus::kBroken);
}

TEST(FormLimits, RejectsAMissingTokenBuffer) {
  // Arrange
  ExchangeHarness harness("a=1", 3U, mado::Method::kPost);
  HarnessExchange exchange = harness.make();

  // Act
  const mado::FormToken token = mado::detail::read_form_token(exchange, '=', nullptr, 8U);

  // Assert
  EXPECT_EQ(token.status, mado::FormStatus::kBroken);
}

TEST(FormLimits, StopsATokenAtTheTokenBound) {
  // Arrange
  const std::string body(0x8001U, 'a');
  ExchangeHarness harness(body, static_cast<uint32_t>(body.size()), mado::Method::kPost);
  HarnessExchange exchange = harness.make();
  static char name[0x9000];

  // Act
  const mado::FormToken token =
      mado::detail::read_form_token(exchange, '=', &name[0], static_cast<uint16_t>(sizeof(name)));

  // Assert
  EXPECT_EQ(token.status, mado::FormStatus::kTooLong);
}

TEST(SendFileLimits, ReportsAClientThatFailsMidBody) {
  // Arrange
  FakeData data(74U);
  FakeFile file(data.word(300));
  ExchangeHarness harness;
  harness.client().fail_writes_after(150U);
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_file(exchange, file, 300U, F("text/plain"), false);

  // Assert
  EXPECT_FALSE(sent);
}

TEST(TemplateLimits, RejectsAMissingTagBuffer) {
  // Arrange
  const mado::detail::FlashPage page = {"$A$", 3U};

  // Act
  const uint8_t length = mado::detail::read_tag(page, 1U, nullptr);

  // Assert
  EXPECT_EQ(length, 0U);
}

TEST(TemplateLimits, RejectsAnEmptyTagName) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const size_t written = mado::detail::emit_tag<HarnessExchange, NoTags>(exchange, "");

  // Assert
  EXPECT_EQ(written, 0U);
}

TEST(TemplateLimits, RejectsAPageLongerThanTheTemplateBound) {
  // Arrange
  const std::string page(0x8001U, 'a');
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();

  // Act
  const bool sent = mado::send_template<HarnessExchange, NoTags>(
      exchange, reinterpret_cast<const __FlashStringHelper*>(page.c_str()));

  // Assert
  EXPECT_FALSE(sent);
}

}
