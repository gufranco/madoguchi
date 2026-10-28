#include <gtest/gtest.h>
#include <mado/LoginGuard.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/eeprom_session.counter.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "../src/identity/application/token_authority.port.h"
#include "../src/identity/domain/credentials.value.h"
#include "FakeData.h"
#include "FakeEeprom.h"
#include "FakeSd.h"
#include "TestSecrets.h"

namespace {

using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;

constexpr uint32_t kClient = 0x0A00000AUL;
constexpr uint32_t kOwner = 0x0B00000BUL;
constexpr const char* kEmptyRows = "00000000 0\n00000000 0\n00000000 0\n";

identity::Credentials credentials_of(const std::string& user, const std::string& password) {
  identity::Credentials credentials = {};
  static_cast<void>(memcpy(&credentials.username[0], user.data(), user.size()));
  static_cast<void>(memcpy(&credentials.password[0], password.data(), password.size()));
  return credentials;
}

class IdentityAdaptersTest : public ::testing::Test {
 protected:
  FakeData& data() {
    return data_;
  }

  FakeSd& card() {
    return card_;
  }

  Access& access() {
    return access_;
  }

  identity::SignInStatus sign_in(const std::string& password, const uint32_t address = kClient) {
    char token[identity::kTokenTextSize] = {};
    return access_.sign_in(credentials_of("admin", password), address, 10U, token);
  }

 private:
  FakeData data_{20261106U};
  FakeSd card_;
  Authority authority_;
  Failures failures_{&card_};
  Access access_{authority_, failures_};
};

TEST_F(IdentityAdaptersTest, StartsTheFirstSessionOnAnErasedChip) {
  // Arrange
  FakeEeprom eeprom;
  identity::EepromSessionCounter<FakeEeprom> sessions(eeprom);

  // Act
  const uint32_t session = sessions.advance();

  // Assert
  EXPECT_EQ(session, 1U);
}

TEST_F(IdentityAdaptersTest, CountsOneSessionPerBoot) {
  // Arrange
  FakeEeprom eeprom;
  identity::EepromSessionCounter<FakeEeprom> sessions(eeprom);
  const uint32_t boots = data().number(2U, 9U);
  for (uint32_t boot = 1U; boot < boots; ++boot) {
    static_cast<void>(sessions.advance());
  }

  // Act
  const uint32_t session = sessions.advance();

  // Assert
  EXPECT_EQ(session, boots);
}

TEST_F(IdentityAdaptersTest, SignsInWithTheRightPassword) {
  // Arrange
  access().begin(7U);

  // Act
  const identity::SignInStatus status = sign_in(&test_secrets::kPassword[0]);

  // Assert
  EXPECT_EQ(status, identity::SignInStatus::kSignedIn);
}

TEST_F(IdentityAdaptersTest, WritesEachFailureToTheCard) {
  // Arrange
  access().begin(7U);
  static_cast<void>(sign_in(data().word(12U)));

  // Act
  static_cast<void>(sign_in(data().word(12U)));

  // Assert
  EXPECT_EQ(card().files()["/LOGIN.TXT"], std::string("0a00000a 2\n") + kEmptyRows);
}

TEST_F(IdentityAdaptersTest, StaysLockedAcrossARestart) {
  // Arrange
  card().files()["/LOGIN.TXT"] = std::string("0a00000a 5\n") + kEmptyRows;

  // Act
  access().begin(8U);

  // Assert
  EXPECT_EQ(access().admit(kClient, 10U), identity::Admission::kLocked);
  EXPECT_EQ(access().admit(kOwner, 10U), identity::Admission::kOpen);
}

TEST_F(IdentityAdaptersTest, AuthorizesTheTokenItSigned) {
  // Arrange
  access().begin(7U);
  char token[identity::kTokenTextSize] = {};
  static_cast<void>(
      access().sign_in(credentials_of("admin", &test_secrets::kPassword[0]), kOwner, 10U, token));
  const std::string header = std::string("Bearer ") + &token[0];
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status = access().authorize(header.c_str(), 20U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kValid);
  EXPECT_EQ(std::string(&claims.subject[0]), "admin");
}

TEST_F(IdentityAdaptersTest, RefusesATokenFromAnEarlierSession) {
  // Arrange
  access().begin(7U);
  char token[identity::kTokenTextSize] = {};
  static_cast<void>(
      access().sign_in(credentials_of("admin", &test_secrets::kPassword[0]), kOwner, 10U, token));
  access().begin(8U);
  const std::string header = std::string("Bearer ") + &token[0];
  mado::TokenClaims claims = {};

  // Act
  const mado::TokenStatus status = access().authorize(header.c_str(), 20U, claims);

  // Assert
  EXPECT_EQ(status, mado::TokenStatus::kStaleSession);
}

TEST_F(IdentityAdaptersTest, LocksOneClientWithoutLockingAnother) {
  // Arrange
  access().begin(7U);
  for (uint8_t attempt = 0U; attempt < mado::LoginGuard::kMaxFailures; ++attempt) {
    static_cast<void>(sign_in(data().word(12U), kClient));
  }

  // Act
  const identity::SignInStatus owner = sign_in(&test_secrets::kPassword[0], kOwner);

  // Assert
  EXPECT_EQ(access().admit(kClient, 10U), identity::Admission::kLocked);
  EXPECT_EQ(owner, identity::SignInStatus::kSignedIn);
}

TEST_F(IdentityAdaptersTest, LocksANewClientWhileEveryTrackedAddressIsLocked) {
  // Arrange
  access().begin(7U);
  for (uint32_t client = 1U; client <= identity::kGuardSlots; ++client) {
    for (uint8_t attempt = 0U; attempt < mado::LoginGuard::kMaxFailures; ++attempt) {
      static_cast<void>(sign_in(data().word(12U), kClient + client));
    }
  }

  // Act
  const identity::Admission newcomer = access().admit(kOwner, 10U);

  // Assert
  EXPECT_EQ(newcomer, identity::Admission::kLocked);
}

}
