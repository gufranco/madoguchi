#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include <string>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/adapters/ram_post.ledger.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, items::RamItemRepository, items::RamPostLedger, FakeSd,
                        FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_post::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestPostTest : public ::testing::Test {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  std::string post(const std::string& type, const std::string& body,
                   const std::string& headers = "") {
    FakeClient client("POST /api/items HTTP/1.1\r\nHost: mado.test\r\n" + bearer() + "" + headers +
                      "Content-Type: " + type +
                      "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
    server_.process(client);
    return client.output();
  }

  FakeData& data() {
    return data_;
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
    App::attach_access(keeper.access, keeper.clock);
    App::attach(repository_, ledger_, &card_);
  }

 private:
  FakeSd card_;
  items::RamItemRepository repository_;
  items::RamPostLedger ledger_;
  AppServer server_{&rest_post::kRoutes[0], &rest_post::kHeaderNames[0]};
  FakeData data_{20261027U};
};

TEST_F(RestPostTest, CreatesAnItemAndPointsAtIt) {
  // Arrange
  const std::string name = data().word(9U);

  // Act
  const std::string response = post("application/json", R"({"name":")" + name + R"(","on":true})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 201 Created");
  EXPECT_NE(response.find("Location: /api/items/1\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"id":1,"name":")" + name + R"(","on":true})");
}

TEST_F(RestPostTest, NumbersItemsInOrder) {
  // Arrange
  static_cast<void>(post("application/json", R"({"name":"a","on":true})"));

  // Act
  const std::string response = post("application/json", R"({"name":"b","on":false})");

  // Assert
  EXPECT_NE(response.find("Location: /api/items/2\r\n"), std::string::npos);
}

TEST_F(RestPostTest, ReportsAFullTable) {
  // Arrange
  for (int slot = 0; slot < 4; ++slot) {
    static_cast<void>(post("application/json", R"({"name":"x","on":true})"));
  }

  // Act
  const std::string response = post("application/json", R"({"name":"y","on":true})");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 507 Insufficient Storage");
}

TEST_F(RestPostTest, ExplainsAMissingRequiredProperty) {
  // Arrange
  const std::string body = R"({"name":")" + data().word(5U) + R"("})";

  // Act
  const std::string response = post("application/json", body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_NE(response.find("Content-Type: application/problem+json\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"({"title":"Unprocessable Content","status":422,)"
                               R"("detail":"missing required property: on"})");
}

TEST_F(RestPostTest, ExplainsAWrongMediaType) {
  // Arrange
  const std::string body = R"({"name":"a","on":true})";

  // Act
  const std::string response = post("text/plain", body);

  // Assert
  EXPECT_EQ(body_of(response), R"({"title":"Unsupported Media Type","status":415,)"
                               R"("detail":"Content-Type must be application/json"})");
}

TEST_F(RestPostTest, RefusesAnEmptyName) {
  // Arrange
  const std::string body = R"({"name":"","on":true})";

  // Act
  const std::string response = post("application/json", body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_NE(response.find(R"("detail":"a string is shorter than its minLength")"),
            std::string::npos);
}

struct PostCase {
  std::string type;
  std::string body;
  const char* expected = nullptr;
};

class RestPostRejectTest : public RestPostTest, public ::testing::WithParamInterface<PostCase> {};

TEST_P(RestPostRejectTest, AnswersTheRightStatus) {
  // Arrange
  const PostCase& sample = GetParam();

  // Act
  const std::string response = post(sample.type, sample.body);

  // Assert
  EXPECT_EQ(status_line(response), sample.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Bodies, RestPostRejectTest,
    ::testing::Values(
        PostCase{"text/plain", R"({"name":"a","on":true})", "HTTP/1.1 415 Unsupported Media Type"},
        PostCase{"application/json", "", "HTTP/1.1 400 Bad Request"},
        PostCase{"application/json", R"({"name":"a",)", "HTTP/1.1 400 Bad Request"},
        PostCase{"application/json", R"({"name":"a"})", "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"name":"a","on":1})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"id":3,"name":"a","on":true})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", R"({"name":"aaaaaaaaaaaaaaaaa","on":true})",
                 "HTTP/1.1 422 Unprocessable Content"},
        PostCase{"application/json", std::string(300U, ' ') + "{}",
                 "HTTP/1.1 413 Content Too Large"}));

std::string keyed(const std::string& key) {
  return "Idempotency-Key: \"" + key + "\"\r\n";
}

std::string named(const std::string& name) {
  return R"({"name":")" + name + R"(","on":true})";
}

TEST_F(RestPostTest, ReplaysARetryWithTheSameKeyAndBody) {
  // Arrange
  const std::string key = data().word(20U);
  const std::string body = named(data().word(6U));
  const std::string first = post("application/json", body, keyed(key));

  // Act
  const std::string retry = post("application/json", body, keyed(key));

  // Assert
  EXPECT_EQ(status_line(first), "HTTP/1.1 201 Created");
  EXPECT_EQ(status_line(retry), "HTTP/1.1 201 Created");
  EXPECT_NE(retry.find("Location: /api/items/1\r\n"), std::string::npos);
  EXPECT_EQ(body_of(retry), body_of(first));
  EXPECT_NE(post("application/json", named("next")).find("Location: /api/items/2\r\n"),
            std::string::npos);
}

TEST_F(RestPostTest, CreatesOneItemForTenIdenticalRetries) {
  // Arrange
  const std::string key = data().word(12U);
  const std::string body = named(data().word(6U));
  std::string last;

  // Act
  for (int attempt = 0; attempt < 10; ++attempt) {
    last = post("application/json", body, keyed(key));
  }

  // Assert
  EXPECT_NE(last.find("Location: /api/items/1\r\n"), std::string::npos);
  EXPECT_NE(post("application/json", named("next")).find("Location: /api/items/2\r\n"),
            std::string::npos);
}

TEST_F(RestPostTest, RefusesAKeySentBeforeWithADifferentBody) {
  // Arrange
  const std::string key = data().word(16U);
  static_cast<void>(post("application/json", named(data().word(5U)), keyed(key)));

  // Act
  const std::string response = post("application/json", named(data().word(7U)), keyed(key));

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 422 Unprocessable Content");
  EXPECT_NE(body_of(response).find("this Idempotency-Key was sent before with a different body"),
            std::string::npos);
}

TEST_F(RestPostTest, RefusesAnUnquotedKey) {
  // Arrange
  const std::string header = "Idempotency-Key: " + data().word(10U) + "\r\n";

  // Act
  const std::string response = post("application/json", named(data().word(5U)), header);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 400 Bad Request");
}

TEST_F(RestPostTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "POST /api/items HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestPostTest, IssuesATokenForTheRightPassword) {
  // Arrange
  const std::string body =
      std::string(R"({"username":"admin","password":")") + &test_secrets::kPassword[0] + R"("})";

  // Act
  const std::string response =
      raw("POST /api/auth/login HTTP/1.1\r\nHost: mado.test\r\nContent-Type: application/json\r\n"
          "Content-Length: " +
          std::to_string(body.size()) + "\r\n\r\n" + body);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(login_body_of(response).find(R"("token_type":"Bearer")"), std::string::npos);
}

}
