#include <Arduino.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>

#include "ExchangeHarness.h"
#include "mado/LoginGuard.h"
#include "mado/MemorySink.h"
#include "mado/TokenClaims.h"
#include "mado/TokenStatus.h"
#include "mado/UptimeClock.h"
#include "mado/issue_token.h"
#include "mado/kTokenKeySize.h"
#include "mado/key_provisioned.h"
#include "mado/valid_token_subject.h"
#include "mado/verify_bearer.h"

namespace {

const uint8_t kKey[mado::kTokenKeySize] PROGMEM = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};
const uint8_t kZeroKey[mado::kTokenKeySize] PROGMEM = {};

const char kValidToken[] =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
    "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQ";

struct ClaimsInput {
  const char* subject = nullptr;
  uint32_t session = 0U;
  uint32_t ends_at = 0U;
};

mado::TokenClaims claims_for(const ClaimsInput& input) {
  mado::TokenClaims claims = {};
  strncpy(&claims.subject[0], input.subject, sizeof(claims.subject) - 1U);
  claims.session = input.session;
  claims.ends_at = input.ends_at;
  return claims;
}

mado::TokenStatus verify(const std::string& authorization, uint32_t session, uint32_t now) {
  mado::TokenClaims claims = {};
  return mado::verify_bearer(authorization.c_str(), &kKey[0], session, now, claims);
}

TEST(IssueToken, MatchesATokenMintedByPython) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  const mado::TokenClaims claims = claims_for(ClaimsInput{"admin", 7U, 900U});

  // Act
  const bool issued = mado::issue_token(exchange, &kKey[0], claims);

  // Assert
  EXPECT_TRUE(issued);
  EXPECT_EQ(harness.sent(), &kValidToken[0]);
}

TEST(IssueToken, EncodesTheLongestClaims) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  const mado::TokenClaims claims =
      claims_for(ClaimsInput{"abcdefghijklmnop", 2147483647U, 2147483647U});

  // Act
  const bool issued = mado::issue_token(exchange, &kKey[0], claims);

  // Assert
  EXPECT_TRUE(issued);
  EXPECT_EQ(harness.sent(),
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhYmNkZWZnaGlqa2xtbm9wIiwic2VzIjo"
            "yMTQ3NDgzNjQ3LCJlbmQiOjIxNDc0ODM2NDd9.7kUdkYcmlY4IRLF88YURWdUrFhtSZfS9AsGhnYSmq2s");
}

TEST(IssueToken, WritesIntoAMemoryBuffer) {
  // Arrange
  char text[201] = {};
  mado::MemorySink sink(&text[0], sizeof(text));
  const mado::TokenClaims claims = claims_for(ClaimsInput{"admin", 7U, 900U});

  // Act
  const bool issued = mado::issue_token(sink, &kKey[0], claims);

  // Assert
  EXPECT_TRUE(issued);
  EXPECT_STREQ(&text[0], &kValidToken[0]);
  EXPECT_EQ(sink.length(), strlen(&kValidToken[0]));
}

TEST(MemorySink, StopsAtTheEndOfTheBuffer) {
  // Arrange
  char text[3] = {};
  mado::MemorySink sink(&text[0], sizeof(text));
  static_cast<void>(sink.write('a'));
  static_cast<void>(sink.write('b'));

  // Act
  const size_t written = sink.write('c');

  // Assert
  EXPECT_EQ(written, 0U);
  EXPECT_STREQ(&text[0], "ab");
}

TEST(MemorySink, RefusesAMissingBuffer) {
  // Arrange
  mado::MemorySink sink(nullptr, 4U);

  // Act
  const size_t written = sink.write('a');

  // Assert
  EXPECT_EQ(written, 0U);
  EXPECT_EQ(sink.length(), 0U);
}

TEST(IssueToken, RefusesAnUnprovisionedKey) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  const mado::TokenClaims claims = claims_for(ClaimsInput{"admin", 7U, 900U});

  // Act
  const bool issued = mado::issue_token(exchange, &kZeroKey[0], claims);

  // Assert
  EXPECT_FALSE(issued);
  EXPECT_EQ(harness.sent(), "");
}

TEST(IssueToken, RefusesASubjectThatNeedsEscaping) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  const mado::TokenClaims claims = claims_for(ClaimsInput{"ad\"min", 7U, 900U});

  // Act
  const bool issued = mado::issue_token(exchange, &kKey[0], claims);

  // Assert
  EXPECT_FALSE(issued);
}

TEST(IssueToken, RefusesAClaimOutsideTheSignedRange) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  const mado::TokenClaims claims = claims_for(ClaimsInput{"admin", 7U, 0x80000000U});

  // Act
  const bool issued = mado::issue_token(exchange, &kKey[0], claims);

  // Assert
  EXPECT_FALSE(issued);
}

TEST(IssueToken, ReportsAFailedOutput) {
  // Arrange
  ExchangeHarness harness;
  HarnessExchange exchange = harness.make();
  harness.detach_output();
  const mado::TokenClaims claims = claims_for(ClaimsInput{"admin", 7U, 900U});

  // Act
  const bool issued = mado::issue_token(exchange, &kKey[0], claims);

  // Assert
  EXPECT_FALSE(issued);
}

TEST(VerifyBearer, AcceptsATokenMintedByPython) {
  // Arrange
  const std::string authorization = std::string("Bearer ") + &kValidToken[0];
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status =
      mado::verify_bearer(authorization.c_str(), &kKey[0], 7U, 899U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kValid);
  EXPECT_STREQ(&claims.subject[0], "admin");
  EXPECT_EQ(claims.session, 7U);
  EXPECT_EQ(claims.ends_at, 900U);
}

TEST(VerifyBearer, AcceptsTheSchemeInAnyCase) {
  // Arrange
  const std::string authorization = std::string("bEaReR ") + &kValidToken[0];

  // Act
  const mado::TokenStatus status = verify(authorization, 7U, 10U);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kValid);
}

TEST(VerifyBearer, ReportsAMissingHeader) {
  // Arrange
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status = mado::verify_bearer(nullptr, &kKey[0], 7U, 10U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kMissing);
}

TEST(VerifyBearer, RefusesEverythingWithoutAProvisionedKey) {
  // Arrange
  const std::string authorization = std::string("Bearer ") + &kValidToken[0];
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status =
      mado::verify_bearer(authorization.c_str(), &kZeroKey[0], 7U, 10U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kUnprovisioned);
}

TEST(VerifyBearer, ExpiresAtTheEndSecond) {
  // Arrange
  const std::string authorization = std::string("Bearer ") + &kValidToken[0];

  // Act
  const mado::TokenStatus status = verify(authorization, 7U, 900U);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kExpired);
}

TEST(VerifyBearer, RejectsATokenFromAnEarlierSession) {
  // Arrange
  const std::string authorization = std::string("Bearer ") + &kValidToken[0];

  // Act
  const mado::TokenStatus status = verify(authorization, 8U, 10U);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kStaleSession);
}

struct RejectCase {
  std::string authorization;
  mado::TokenStatus expected = mado::TokenStatus::kValid;
};

class VerifyBearerRejectTest : public ::testing::TestWithParam<RejectCase> {};

TEST_P(VerifyBearerRejectTest, RejectsTheToken) {
  // Arrange
  const RejectCase& sample = GetParam();

  // Act
  const mado::TokenStatus status = verify(sample.authorization, 7U, 10U);

  // Assert
  EXPECT_EQ(status, sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Tampered, VerifyBearerRejectTest,
    ::testing::Values(
        RejectCase{std::string("Basic ") + &kValidToken[0], mado::TokenStatus::kMalformed},
        RejectCase{std::string("Bearer  ") + &kValidToken[0], mado::TokenStatus::kMalformed},
        RejectCase{"Bearer", mado::TokenStatus::kMalformed},
        RejectCase{"Bearer ", mado::TokenStatus::kMalformed},
        RejectCase{"Bearer a.b", mado::TokenStatus::kMalformed},
        RejectCase{"Bearer a.b.c.d", mado::TokenStatus::kMalformed},
        RejectCase{"Bearer ..", mado::TokenStatus::kMalformed},
        RejectCase{std::string("Bearer ") + &kValidToken[0] + std::string(200U, 'A'),
                   mado::TokenStatus::kMalformed},
        RejectCase{"Bearer eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9.",
                   mado::TokenStatus::kMalformed},
        RejectCase{"Bearer eyJhbGciOiJIUzM4NCIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
                   "k9pLitPhZF46WzTMcSpQSYTX1YkIrFGFuxxp8wz9480",
                   mado::TokenStatus::kMalformed},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
                   "SUxY-YOfbl6UPICWlBawWhdSt16BbmGk79sf7cg8pyw",
                   mado::TokenStatus::kBadSignature},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6OCwiZW5kIjo5MDB9."
                   "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQ",
                   mado::TokenStatus::kBadSignature},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
                   "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDR",
                   mado::TokenStatus::kBadSignature},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
                   "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJD",
                   mado::TokenStatus::kBadSignature},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDAsImFkbSI6dHJ1ZX0."
                   "8oQcEfujg_BPEUWVt671jXZtWa5aZ0fEQcAT2AyVGXQ",
                   mado::TokenStatus::kMalformed},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6N30."
                   "0l-G1j3fXNDc1mrhLA1peVLgytFHKg2yhuynw5HzleA",
                   mado::TokenStatus::kMalformed},
        RejectCase{"Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                   "eyJzdWIiOiJhZG1pbiIsInNlcyI6LTEsImVuZCI6OTAwfQ."
                   "qLB4jqg19z1Br9YY7dGhn70o2RdBAXjqA4elUGlvbCs",
                   mado::TokenStatus::kMalformed}));

TEST(KeyProvisioned, RejectsAnAllZeroKey) {
  // Arrange
  const uint8_t* const key = &kZeroKey[0];

  // Act
  const bool provisioned = mado::key_provisioned(key);

  // Assert
  EXPECT_FALSE(provisioned);
}

TEST(KeyProvisioned, RejectsAMissingKey) {
  // Arrange
  const uint8_t* const key = nullptr;

  // Act
  const bool provisioned = mado::key_provisioned(key);

  // Assert
  EXPECT_FALSE(provisioned);
}

class ValidTokenSubjectTest : public ::testing::TestWithParam<std::pair<const char*, bool>> {};

TEST_P(ValidTokenSubjectTest, AllowsOnlySafeCharacters) {
  // Arrange
  const std::pair<const char*, bool>& sample = GetParam();

  // Act
  const bool valid = mado::valid_token_subject(sample.first);

  // Assert
  EXPECT_EQ(valid, sample.second);
}

INSTANTIATE_TEST_SUITE_P(
    Subjects, ValidTokenSubjectTest,
    ::testing::Values(std::make_pair("admin", true), std::make_pair("Admin", true),
                      std::make_pair("a`b", false), std::make_pair("a~", false),
                      std::make_pair("a@", false), std::make_pair("a.b_c-9", true),
                      std::make_pair("abcdefghijklmnop", true),
                      std::make_pair("abcdefghijklmnopq", false), std::make_pair("", false),
                      std::make_pair("a b", false), std::make_pair("a\\b", false),
                      std::make_pair(static_cast<const char*>(nullptr), false)));

TEST(UptimeClock, CountsWholeSeconds) {
  // Arrange
  mado::UptimeClock clock;
  clock.advance(999U);

  // Act
  clock.advance(1500U);

  // Assert
  EXPECT_EQ(clock.seconds(), 1U);
}

TEST(UptimeClock, KeepsCountingAcrossTheMillisWrap) {
  // Arrange
  mado::UptimeClock clock;
  clock.advance(0xFFFFFC18U);
  const uint32_t before = clock.seconds();

  // Act
  clock.advance(1000U);

  // Assert
  EXPECT_EQ(before, 4294966U);
  EXPECT_EQ(clock.seconds(), 4294968U);
}

TEST(UptimeClock, CarriesLeftoverMilliseconds) {
  // Arrange
  mado::UptimeClock clock;
  clock.advance(600U);
  clock.advance(1200U);

  // Act
  clock.advance(1800U);

  // Assert
  EXPECT_EQ(clock.seconds(), 1U);
}

TEST(LoginGuard, LocksAfterFiveFailures) {
  // Arrange
  mado::LoginGuard guard;
  for (uint32_t i = 0U; i < 4U; ++i) {
    guard.record_failure(100U + i);
  }

  // Act
  guard.record_failure(104U);

  // Assert
  EXPECT_TRUE(guard.locked(104U));
  EXPECT_EQ(guard.retry_after(104U), 900U);
  EXPECT_EQ(guard.retry_after(1003U), 1U);
}

TEST(LoginGuard, RestoresALockFromStorage) {
  // Arrange
  mado::LoginGuard guard;

  // Act
  guard.restore(5U);

  // Assert
  EXPECT_TRUE(guard.locked(0U));
  EXPECT_EQ(guard.retry_after(0U), 900U);
  EXPECT_EQ(guard.failures(), 5U);
}

TEST(LoginGuard, TreatsAnErasedCellAsNoFailures) {
  // Arrange
  mado::LoginGuard guard;

  // Act
  guard.restore(mado::LoginGuard::kErasedCell);

  // Assert
  EXPECT_FALSE(guard.locked(0U));
  EXPECT_EQ(guard.failures(), 0U);
}

TEST(LoginGuard, LocksOnACorruptStoredCount) {
  // Arrange
  mado::LoginGuard guard;

  // Act
  guard.restore(9U);

  // Assert
  EXPECT_TRUE(guard.locked(50U));
  EXPECT_EQ(guard.failures(), 5U);
}

TEST(LoginGuard, IgnoresARestoreAfterUse) {
  // Arrange
  mado::LoginGuard guard;
  guard.record_failure(10U);

  // Act
  guard.restore(5U);

  // Assert
  EXPECT_FALSE(guard.locked(20U));
  EXPECT_EQ(guard.failures(), 1U);
}

TEST(LoginGuard, UnlocksAfterFifteenMinutes) {
  // Arrange
  mado::LoginGuard guard;
  for (uint32_t i = 0U; i < 5U; ++i) {
    guard.record_failure(10U);
  }

  // Act
  const bool locked = guard.locked(910U);

  // Assert
  EXPECT_FALSE(locked);
  EXPECT_EQ(guard.retry_after(910U), 0U);
}

TEST(LoginGuard, IgnoresFailuresWhileLocked) {
  // Arrange
  mado::LoginGuard guard;
  for (uint32_t i = 0U; i < 5U; ++i) {
    guard.record_failure(10U);
  }

  // Act
  guard.record_failure(500U);

  // Assert
  EXPECT_EQ(guard.retry_after(500U), 410U);
}

TEST(LoginGuard, StartsCountingAgainAfterTheLockEnds) {
  // Arrange
  mado::LoginGuard guard;
  for (uint32_t i = 0U; i < 5U; ++i) {
    guard.record_failure(10U);
  }

  // Act
  guard.record_failure(2000U);

  // Assert
  EXPECT_FALSE(guard.locked(2000U));
}

TEST(LoginGuard, ForgetsFailuresAfterASuccess) {
  // Arrange
  mado::LoginGuard guard;
  for (uint32_t i = 0U; i < 4U; ++i) {
    guard.record_failure(10U);
  }
  guard.record_success();

  // Act
  guard.record_failure(11U);

  // Assert
  EXPECT_FALSE(guard.locked(11U));
}

}
