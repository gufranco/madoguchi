#include <gtest/gtest.h>
#include <stdint.h>

#include <string>

#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "mado/DefaultLimits.h"
#include "mado/HeaderParser.h"
#include "mado/HeaderPool.h"
#include "mado/HeaderStatus.h"
#include "mado/HeaderSummary.h"

namespace {

using Limits = mado::DefaultLimits;
using Parser = mado::HeaderParser<Limits, FakeClient>;

const char* const* names() {
  static const char* const kNames[] = {"Host", "X-Long", nullptr};
  return &kNames[0];
}

class Parsed {
 public:
  explicit Parsed(const std::string& headers) : client_(headers) {
    fake_clock::set(0U);
  }

  mado::HeaderSummary run() {
    Parser parser(client_, names(), pool_, 0U);
    return parser.parse();
  }

  std::string captured(uint8_t slot) const {
    const char* value = pool_.value(slot);
    return value == nullptr ? std::string("<none>") : std::string(value);
  }

  FakeClient& client() {
    return client_;
  }

 private:
  FakeClient client_;
  mado::HeaderPool<Limits> pool_;
};

struct LengthCase {
  const char* headers;
  mado::HeaderStatus status;
  uint32_t length;
};

class ContentLengthTest : public ::testing::TestWithParam<LengthCase> {};

TEST_P(ContentLengthTest, ParsesOnlyAWellFormedDecimalLength) {
  // Arrange
  const LengthCase sample = GetParam();
  Parsed parsed(sample.headers);

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, sample.status);
  EXPECT_EQ(summary.content_length, sample.length);
}

INSTANTIATE_TEST_SUITE_P(
    Lengths, ContentLengthTest,
    ::testing::Values(
        LengthCase{"Content-Length: 49209\r\n\r\n", mado::HeaderStatus::kOk, 49209U},
        LengthCase{"content-length:7  \r\n\r\n", mado::HeaderStatus::kOk, 7U},
        LengthCase{"Content-Length: 4294967295\r\n\r\n", mado::HeaderStatus::kOk, 4294967295U},
        LengthCase{"Content-Length: 12 34\r\n\r\n", mado::HeaderStatus::kMalformed, 12U},
        LengthCase{"Content-Length: 1x\r\n\r\n", mado::HeaderStatus::kMalformed, 1U},
        LengthCase{"Content-Length: \r\n\r\n", mado::HeaderStatus::kMalformed, 0U},
        LengthCase{"Content-Length: 42949672950\r\n\r\n", mado::HeaderStatus::kMalformed,
                   4294967295U}));

struct ExpectCase {
  const char* headers;
  bool expected;
};

class ExpectTest : public ::testing::TestWithParam<ExpectCase> {};

TEST_P(ExpectTest, RecognizesOnlyAnExactContinueExpectation) {
  // Arrange
  const ExpectCase sample = GetParam();
  Parsed parsed(sample.headers);

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kOk);
  EXPECT_EQ(summary.expect_continue, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(Expectations, ExpectTest,
                         ::testing::Values(ExpectCase{"Expect: 100-continue\r\n\r\n", true},
                                           ExpectCase{"expect: 100-Continue\r\n\r\n", true},
                                           ExpectCase{"Expect: 100-continued\r\n\r\n", false},
                                           ExpectCase{"Expect: 100-cont\r\n\r\n", false},
                                           ExpectCase{"Expect: 100 continue\r\n\r\n", false},
                                           ExpectCase{"Expect: 200-ok\r\n\r\n", false}));

TEST(HeaderParser, JoinsInnerWhitespaceInACapturedValue) {
  // Arrange
  Parsed parsed("X-Long:   a \t b  \r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kOk);
  EXPECT_EQ(parsed.captured(1U), "a b");
}

TEST(HeaderParser, TruncatesACapturedValueAtThePoolEnd) {
  // Arrange
  FakeData data(21U);
  const std::string value = data.word(80);
  Parsed parsed("X-Long: " + value + "\r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kOk);
  EXPECT_EQ(parsed.captured(1U), value.substr(0U, 47U));
}

TEST(HeaderParser, SkipsACaptureWhenThePoolIsFull) {
  // Arrange
  FakeData data(22U);
  Parsed parsed("X-Long: " + data.word(80) + "\r\nHost: " + data.word(5) + "\r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kOk);
  EXPECT_EQ(parsed.captured(0U), "<none>");
}

TEST(HeaderParser, IgnoresANameLongerThanTheNameBuffer) {
  // Arrange
  FakeData data(23U);
  Parsed parsed("X-" + data.word(60) + ": " + data.word(4) + "\r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kOk);
}

struct MalformedCase {
  const char* headers;
};

class MalformedHeaderTest : public ::testing::TestWithParam<MalformedCase> {};

TEST_P(MalformedHeaderTest, RejectsBrokenHeaderLines) {
  // Arrange
  const MalformedCase sample = GetParam();
  Parsed parsed(sample.headers);

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kMalformed);
}

INSTANTIATE_TEST_SUITE_P(Lines, MalformedHeaderTest,
                         ::testing::Values(MalformedCase{": value\r\n\r\n"},
                                           MalformedCase{"Name\r\n\r\n"},
                                           MalformedCase{"Bad Name: x\r\n\r\n"},
                                           MalformedCase{"Na\rme: x\r\n\r\n"}));

TEST(HeaderParser, ReportsATimeoutInsideAHeaderName) {
  // Arrange
  Parsed parsed("Host");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kTimeout);
}

TEST(HeaderParser, ReportsATimeoutInsideAHeaderValue) {
  // Arrange
  Parsed parsed("Host: abc");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kTimeout);
}

TEST(HeaderParser, ReportsAClosedConnection) {
  // Arrange
  Parsed parsed("Host: abc");
  parsed.client().disconnect();

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kClosed);
}

TEST(HeaderParser, RejectsAValueLongerThanTheScanLimit) {
  // Arrange
  FakeData data(24U);
  Parsed parsed("X-Other: " + data.word(1100) + "\r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kMalformed);
}

TEST(HeaderParser, RejectsANameLongerThanTheScanLimit) {
  // Arrange
  FakeData data(25U);
  Parsed parsed(data.word(1100) + ": x\r\n\r\n");

  // Act
  const mado::HeaderSummary summary = parsed.run();

  // Assert
  EXPECT_EQ(summary.status, mado::HeaderStatus::kMalformed);
}

}
