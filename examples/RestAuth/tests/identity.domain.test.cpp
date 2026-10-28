#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/greetings/domain/greeting.value.h"
#include "../src/identity/domain/credentials.value.h"
#include "../src/identity/domain/session.value.h"
#include "FakeData.h"

namespace {

identity::Credentials credentials_of(const std::string& user, const std::string& password) {
  identity::Credentials credentials = {};
  static_cast<void>(memcpy(&credentials.username[0], user.data(), user.size()));
  static_cast<void>(memcpy(&credentials.password[0], password.data(), password.size()));
  return credentials;
}

class IdentityDomainTest : public ::testing::Test {
 protected:
  FakeData data_{20261105U};
};

TEST_F(IdentityDomainTest, CallsCredentialsWithBothPartsComplete) {
  // Arrange
  const identity::Credentials credentials = credentials_of(data_.word(5U), data_.word(20U));

  // Act
  const identity::CredentialsStatus status = identity::credentials_status(credentials);

  // Assert
  EXPECT_EQ(status, identity::CredentialsStatus::kComplete);
}

TEST_F(IdentityDomainTest, CallsCredentialsWithAnEmptyPasswordEmpty) {
  // Arrange
  const identity::Credentials credentials = credentials_of(data_.word(5U), "");

  // Act
  const identity::CredentialsStatus status = identity::credentials_status(credentials);

  // Assert
  EXPECT_EQ(status, identity::CredentialsStatus::kEmpty);
}

TEST_F(IdentityDomainTest, CallsAUserNameWithNoTerminatorUnterminated) {
  // Arrange
  const identity::Credentials credentials =
      credentials_of(data_.word(identity::kUserNameSize), data_.word(8U));

  // Act
  const identity::CredentialsStatus status = identity::credentials_status(credentials);

  // Assert
  EXPECT_EQ(status, identity::CredentialsStatus::kUnterminated);
}

struct SessionCase {
  uint32_t stored;
  uint32_t next;
};

class NextSessionTest : public ::testing::TestWithParam<SessionCase> {};

TEST_P(NextSessionTest, CountsSessionsFromOneAndWrapsAtTheClaimLimit) {
  // Arrange
  const SessionCase sample = GetParam();

  // Act
  const uint32_t next = identity::next_session(sample.stored);

  // Assert
  EXPECT_EQ(next, sample.next);
}

INSTANTIATE_TEST_SUITE_P(Counter, NextSessionTest,
                         ::testing::Values(SessionCase{0U, 1U}, SessionCase{1U, 2U},
                                           SessionCase{0x7FFFFFFEU, 0x7FFFFFFFU},
                                           SessionCase{0x7FFFFFFFU, 1U},
                                           SessionCase{0xFFFFFFFFU, 1U}));

TEST_F(IdentityDomainTest, EndsATokenFifteenMinutesLater) {
  // Arrange
  const uint32_t now = data_.number(0U, 100000U);
  uint32_t ends_at = 0U;

  // Act
  const bool timed = identity::token_ends_at(now, ends_at);

  // Assert
  EXPECT_TRUE(timed);
  EXPECT_EQ(ends_at, now + 900U);
}

TEST_F(IdentityDomainTest, RefusesATokenThatWouldEndPastTheClaimLimit) {
  // Arrange
  uint32_t ends_at = 1U;

  // Act
  const bool timed = identity::token_ends_at(0x7FFFFFFFU, ends_at);

  // Assert
  EXPECT_FALSE(timed);
  EXPECT_EQ(ends_at, 0U);
}

TEST_F(IdentityDomainTest, GreetsASubjectByName) {
  // Arrange
  const std::string subject = data_.word(6U);
  greetings::Greeting greeting = {};

  // Act
  const bool greeted = greetings::greet(subject.c_str(), greeting);

  // Assert
  EXPECT_TRUE(greeted);
  EXPECT_EQ(std::string(&greeting.message[0]), "Hello, " + subject);
}

TEST_F(IdentityDomainTest, RefusesAGreetingThatDoesNotFit) {
  // Arrange
  const std::string subject = data_.word(greetings::kMessageSize);
  greetings::Greeting greeting = {};

  // Act
  const bool greeted = greetings::greet(subject.c_str(), greeting);

  // Assert
  EXPECT_FALSE(greeted);
  EXPECT_EQ(greeting.message[0], '\0');
}

}
