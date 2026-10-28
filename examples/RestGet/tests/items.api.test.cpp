#include <Madoguchi.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <string.h>

#include <string>

#include "../src/http/api.generated.h"
#include "../src/http/api.router.h"
#include "../src/identity/adapters/card_failure.store.h"
#include "../src/identity/adapters/flash_secrets.authority.h"
#include "../src/identity/application/access.service.h"
#include "../src/items/adapters/ram_item.repository.h"
#include "../src/items/application/item_catalog.service.h"
#include "../src/items/domain/item.entity.h"
#include "FakeClient.h"
#include "FakeClock.h"
#include "FakeData.h"
#include "FakeSd.h"
#include "TestAccess.h"
#include "TestSecrets.h"

namespace {

constexpr const char* kSpecText = R"({"openapi":"3.1.0"})";

using Catalog = items::ItemCatalog<items::RamItemRepository>;
using Authority = identity::FlashSecretsAuthority<test_secrets::TestSecrets>;
using Failures = identity::CardFailureLog<FakeSd, FILE_READ, FILE_WRITE>;
using Access = identity::AccessService<Authority, Failures>;
using Gatekeeper = TestAccess<Authority, Failures, Access>;
using App = api::Router<FakeClient, Access, items::RamItemRepository, FakeSd, FILE_READ>;
using AppServer = mado::Server<FakeClient, App, rest_get::Limits>;

std::string body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

std::string status_line(const std::string& response) {
  return response.substr(0U, response.find("\r\n"));
}

items::ItemDraft draft_named(const std::string& name, const bool switched_on) {
  items::ItemDraft draft = {};
  static_cast<void>(memcpy(&draft.name[0], name.data(), name.size()));
  draft.on = switched_on;
  return draft;
}

std::string login_body_of(const std::string& response) {
  const size_t split = response.find("\r\n\r\n");
  return (split == std::string::npos) ? "" : response.substr(split + 4U);
}

class RestGetTest : public ::testing::Test {
 public:
  std::string raw(const std::string& wire) {
    FakeClient client(wire);
    server_.process(client);
    return client.output();
  }

  std::string send(const std::string& line) {
    FakeClient client(line + "\r\nHost: mado.test\r\n" + bearer() +
                      "Accept-Encoding: gzip\r\n\r\n");
    server_.process(client);
    return client.output();
  }

 protected:
  void SetUp() override {
    fake_clock::set(0U);
    card_.files()["/openapi.json"] = kSpecText;
    static_cast<void>(Catalog::add(repository_, draft_named(first_, true)));
    static_cast<void>(Catalog::add(repository_, draft_named(second_, false)));
    Gatekeeper& keeper = fresh_gatekeeper<Gatekeeper>();
    App::attach_access(keeper.access, keeper.clock);
    App::attach(repository_, &card_);
  }

  const std::string& first() const {
    return first_;
  }

  const std::string& second() const {
    return second_;
  }

  FakeData& data() {
    return data_;
  }

  FakeSd& card() {
    return card_;
  }

  items::RamItemRepository& repository() {
    return repository_;
  }

 private:
  FakeData data_{20261030U};
  const std::string first_ = data_.word(4U);
  const std::string second_ = data_.word(3U);
  FakeSd card_;
  items::RamItemRepository repository_;
  AppServer server_{&rest_get::kRoutes[0], &rest_get::kHeaderNames[0]};
};

TEST_F(RestGetTest, ListsTheSeededItems) {
  // Arrange
  const std::string line = "GET /api/items HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), R"([{"id":1,"name":")" + first() +
                                   R"(","on":true},{"id":2,"name":")" + second() +
                                   R"(","on":false}])");
}

TEST_F(RestGetTest, ReadsOneItem) {
  // Arrange
  const std::string line = "GET /api/items/2 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), R"({"id":2,"name":")" + second() + R"(","on":false})");
}

TEST_F(RestGetTest, SendsOnlyHeadersForHead) {
  // Arrange
  const std::string line = "HEAD /api/items/1 HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), "");
}

class RestGetMissingTest : public RestGetTest, public ::testing::WithParamInterface<const char*> {};

TEST_P(RestGetMissingTest, AnswersNotFound) {
  // Arrange
  const std::string line = GetParam();

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
  EXPECT_NE(response.find(R"("detail":"no item has this id")"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(Paths, RestGetMissingTest,
                         ::testing::Values("GET /api/items/3 HTTP/1.1", "GET /api/items/0 HTTP/1.1",
                                           "GET /api/items/x HTTP/1.1"));

TEST_F(RestGetTest, ListsTheOnlyAllowedMethods) {
  // Arrange
  const std::string line = "POST /api/items HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 405 Method Not Allowed");
  EXPECT_NE(response.find("Allow: GET, HEAD\r\n"), std::string::npos);
}

TEST_F(RestGetTest, ServesItsOwnSpec) {
  // Arrange
  const std::string line = "GET /api/openapi.json HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
  EXPECT_NE(response.find("Cache-Control: no-cache\r\n"), std::string::npos);
  EXPECT_EQ(body_of(response), kSpecText);
}

TEST_F(RestGetTest, AnswersNotFoundForAPageMissingFromTheCard) {
  // Arrange
  const std::string line = "GET / HTTP/1.1";

  // Act
  const std::string response = send(line);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 404 Not Found");
}

TEST_F(RestGetTest, ServesAPageFromTheCard) {
  // Arrange
  const std::string page = "<!doctype html><title>" + data().word(5U) + "</title>";
  card().files()["/index.html"] = page;

  // Act
  const std::string response = send("GET / HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 200 OK");
  EXPECT_EQ(body_of(response), page);
}

TEST_F(RestGetTest, AnswersUnavailableWithoutACard) {
  // Arrange
  App::attach(repository(), nullptr);

  // Act
  const std::string response = send("GET /api/openapi.json HTTP/1.1");

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 503 Service Unavailable");
  EXPECT_NE(response.find(R"("detail":"the SD card is not available")"), std::string::npos);
}

TEST_F(RestGetTest, RefusesAnApiRequestWithoutAToken) {
  // Arrange
  const std::string wire = "GET /api/items HTTP/1.1\r\nHost: mado.test\r\n\r\n";

  // Act
  const std::string response = raw(wire);

  // Assert
  EXPECT_EQ(status_line(response), "HTTP/1.1 401 Unauthorized");
  EXPECT_NE(response.find("WWW-Authenticate: Bearer"), std::string::npos);
}

TEST_F(RestGetTest, IssuesATokenForTheRightPassword) {
  // Arrange
  const std::string body =
      std::string(R"({"username":"admin","password":")") + test_secrets::kPassword + R"("})";

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
