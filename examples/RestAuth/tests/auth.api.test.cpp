#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include <mado/UptimeClock.h>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "TestSecrets.h"

namespace {

FakeSd& store() {
  static FakeSd instance;
  return instance;
}

using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using App = api::Router<FakeClient, Access, FakeSd, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_auth::Limits>;
using test_secrets::kPassword;
using test_secrets::kToken;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

class RestAuthTest : public ::testing::Test {
 public:
  std::string login(const std::string& password) {
    const std::string body = R"({"username":"admin","password":")" + password + R"("})";
    return send(
        "POST /api/auth/login HTTP/1.1\r\nHost: mado.test\r\nContent-Type: application/json\r\n"
        "Content-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body);
  }

  std::string greet(const std::string& token) {
    return send("GET /api/greeting HTTP/1.1\r\nHost: mado.test\r\nAuthorization: Bearer " + token +
                "\r\n\r\n");
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    store() = FakeSd();
    access_.begin(7U);
    App::attach(access_, uptime_, &store());
  }

 private:
  std::string send(const std::string& raw) {
    FakeClient client(raw);
    server_.process(client);
    return client.output();
  }

  Authority authority_;
  Failures failures_{&store()};
  Access access_{authority_, failures_};
  mado::UptimeClock uptime_;
  AppServer server_{&rest_auth::kRoutes[0], &rest_auth::kHeaderNames[0]};
  FakeData data_{20261030U};
};

TEST_F(RestAuthTest, IssuesATokenForTheRightPassword) {
  // Arrange
  const std::string password = &kPassword[0];

  // Act
  const std::string response = login(password);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), std::string(R"({"access_token":")") + &kToken[0] +
                                   R"(","token_type":"Bearer","expires_in":900})");
}

TEST_F(RestAuthTest, RefusesTheWrongPassword) {
  // Arrange
  const std::string password = data().word(16U);

  // Act
  const std::string response = login(password);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
}

TEST_F(RestAuthTest, LocksAfterFiveFailures) {
  // Arrange
  for (int attempt = 0; attempt < 5; ++attempt) {
    static_cast<void>(login(data().word(16U)));
  }

  // Act
  const std::string response = login(&kPassword[0]);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 429 Too Many Requests");
}

TEST_F(RestAuthTest, GreetsTheTokenHolder) {
  // Arrange
  const std::string token = &kToken[0];

  // Act
  const std::string response = greet(token);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"message":"Hello, admin"})");
}

TEST_F(RestAuthTest, RefusesAMissingToken) {
  // Arrange
  const std::string token;

  // Act
  const std::string response = greet(token);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
}

TEST_F(RestAuthTest, RefusesATamperedToken) {
  // Arrange
  std::string token = &kToken[0];
  token.back() = (token.back() == 'A') ? 'B' : 'A';

  // Act
  const std::string response = greet(token);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("error=\"invalid_token\""), std::string::npos);
}

TEST_F(RestAuthTest, RefusesATokenAfterFifteenMinutes) {
  // Arrange
  fake_clock::set(900000U);

  // Act
  const std::string response = greet(&kToken[0]);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
}

}
